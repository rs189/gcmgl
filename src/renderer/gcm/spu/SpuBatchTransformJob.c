//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include <stdbool.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "SpuBatchTransformJob.h"

#define JOB_TAG 0u
#define DATA_TAG0 1u
#define DATA_TAG1 2u
#define MATRIX_TAG0 3u
#define MATRIX_TAG1 4u

#define MAX_FLOATS_PER_VERTEX 4u
#define MAX_VERTICES_PER_BLOCK 256u

static const vec_uchar16 SplatXPattern = {
	0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3
};
static const vec_uchar16 SplatYPattern = {
	4,5,6,7, 4,5,6,7, 4,5,6,7, 4,5,6,7
};
static const vec_uchar16 SplatZPattern = {
	8,9,10,11, 8,9,10,11, 8,9,10,11, 8,9,10,11
};
static const vec_uchar16 SplatWPattern = {
	12,13,14,15, 12,13,14,15, 12,13,14,15, 12,13,14,15
};

static SpuBatchJob_t g_Job __attribute__((aligned(16)));
static float32 g_MatrixBuffer[2][16] __attribute__((aligned(16)));
static float32 g_PositionBuffer[2][MAX_VERTICES_PER_BLOCK * MAX_FLOATS_PER_VERTEX] 
	__attribute__((aligned(16)));

static inline void waitForTag(uint32 mask)
{
	mfc_write_tag_mask(mask);
	spu_mfcstat(MFC_TAG_UPDATE_ALL);
}

static inline void dmaGetMatrix(uint32 bufIdx, uint64 addr, uint32 tag)
{
	mfc_get(g_MatrixBuffer[bufIdx], addr, 64, tag, 0, 0);
}

static inline void dmaGetVertices(
	uint32 bufIdx,
	uint64 addr,
	uint32 bytes,
	uint32 tag)
{
	mfc_get(g_PositionBuffer[bufIdx], addr, bytes, tag, 0, 0);
}

static inline void dmaPutVertices(
	uint32 bufIdx,
	uint64 addr,
	uint32 bytes,
	uint32 tag)
{
	mfc_put(g_PositionBuffer[bufIdx], addr, bytes, tag, 0, 0);
}

static inline void transformVertex4(
	vec_float4* pVertex,
	const vec_float4 pMatrix[4])
{
	vec_float4 v = *pVertex;

	vec_float4 x = spu_shuffle(v, v, SplatXPattern);
	vec_float4 y = spu_shuffle(v, v, SplatYPattern);
	vec_float4 z = spu_shuffle(v, v, SplatZPattern);
	vec_float4 w = spu_shuffle(v, v, SplatWPattern);

	vec_float4 result = spu_mul(pMatrix[0], x);
	result = spu_madd(pMatrix[1], y, result);
	result = spu_madd(pMatrix[2], z, result);
	result = spu_madd(pMatrix[3], w, result);

	*pVertex = result;
}

static inline void transformVertex3(
	vec_float4* pVertex,
	const vec_float4 pMatrix[4])
{
	vec_float4 v = *pVertex;
	
	vec_float4 x = spu_shuffle(v, v, SplatXPattern);
	vec_float4 y = spu_shuffle(v, v, SplatYPattern);
	vec_float4 z = spu_shuffle(v, v, SplatZPattern);
	
	vec_float4 result = spu_mul(pMatrix[0], x);
	result = spu_madd(pMatrix[1], y, result);
	result = spu_madd(pMatrix[2], z, result);
	result = spu_add(result, pMatrix[3]);

	*pVertex = result;
}

static void transformBlock4Component(
	float32* pVertexBlock,
	uint32 count,
	const vec_float4 pMatrix[4])
{
	vec_float4* pVertices = (vec_float4*)pVertexBlock;

	uint32 i = 0;
	// Round down to a multiple of 4
	const uint32 unrollCount = (count >> 2u) << 2u;

	// Transform vertices 4 in wide unroll
	for (; i < unrollCount; i += 4u)
	{
		transformVertex4(&pVertices[i + 0u], pMatrix);
		transformVertex4(&pVertices[i + 1u], pMatrix);
		transformVertex4(&pVertices[i + 2u], pMatrix);
		transformVertex4(&pVertices[i + 3u], pMatrix);
	}

	// Transform remaining vertices
	for (; i < count; i++)
	{
		transformVertex4(&pVertices[i], pMatrix);
	}
}

static void transformBlock3Component(
	float32* pVertexBlock, uint32 count, const vec_float4 pMatrix[4])
{
	vec_float4* pVertices = (vec_float4*)pVertexBlock;

	uint32 i = 0;
	// Round down to a multiple of 4
	const uint32 unrollCount = (count >> 2u) << 2u;

	// Transform vertices 4 in wide unroll
	for (; i < unrollCount; i += 4u)
	{
		transformVertex3(&pVertices[i + 0u], pMatrix);
		transformVertex3(&pVertices[i + 1u], pMatrix);
		transformVertex3(&pVertices[i + 2u], pMatrix);
		transformVertex3(&pVertices[i + 3u], pMatrix);
	}
	
	// Transform remaining vertices
	for (; i < count; i++)
	{
		transformVertex3(&pVertices[i], pMatrix);
	}
}

static void processTransforms()
{
	const uint32 vertexCount = g_Job.m_VertexCount;
	const uint32 batchCount = g_Job.m_BatchCount;
	const uint32 floatsPerVertex = g_Job.m_FloatsPerVertex;

	if (floatsPerVertex == 0u || floatsPerVertex > MAX_FLOATS_PER_VERTEX)
	{
		g_Job.m_Status = SPU_BATCH_STATUS_ERROR;

		return;
	}

	const uint32 floatSize = sizeof(float32);
	const bool hasVertexW = (floatsPerVertex > 3u);
	
	for (uint32 batch = 0; batch < batchCount; batch++)
	{
		const uint32 matrixBufIdx = batch & 1u;
		const uint32 nextMatrixBufIdx = (batch + 1u) & 1u;
		const uint32 matrixTag = matrixBufIdx ? MATRIX_TAG1 : MATRIX_TAG0;
		const uint32 nextMatrixTag = nextMatrixBufIdx ? MATRIX_TAG1 : MATRIX_TAG0;

		const uint64 matrixEffAddr = g_Job.m_MatricesEffAddr + ((uint64)batch * g_Job.m_MatrixStride);

		// DMA get current matrix
		dmaGetMatrix(matrixBufIdx, matrixEffAddr, matrixTag);

		// DMA get next matrix
		if (batch + 1u < batchCount)
		{
			const uint64 nextMatrixEffAddr = g_Job.m_MatricesEffAddr + ((uint64)(batch + 1u) * g_Job.m_MatrixStride);
			dmaGetMatrix(nextMatrixBufIdx, nextMatrixEffAddr, nextMatrixTag);
		}

		// Wait for DMA get so matrix is resident
		waitForTag(1u << matrixTag);

		const vec_float4* pMatrixVec = (const vec_float4*)g_MatrixBuffer[matrixBufIdx];
		vec_float4 matrix[4];
		matrix[0] = pMatrixVec[0]; // [0..3]
		matrix[1] = pMatrixVec[1]; // [4..7]
		matrix[2] = pMatrixVec[2]; // [8..11]
		matrix[3] = pMatrixVec[3]; // [12..15]

		// Transform vertices in blocks
		uint32 blockIdx = 0u;
		for (uint32 v = 0; v < vertexCount; v += MAX_VERTICES_PER_BLOCK, blockIdx++)
		{
			const uint32 bufIdx = blockIdx & 1u;
			const uint32 nextBufIdx = (blockIdx + 1u) & 1u;
			const uint32 dataTag = bufIdx ? DATA_TAG1 : DATA_TAG0;
			const uint32 nextDataTag = nextBufIdx ? DATA_TAG1 : DATA_TAG0;
			
			const uint32 remaining = vertexCount - v;
			const uint32 currentCount = (remaining < MAX_VERTICES_PER_BLOCK) ? remaining : MAX_VERTICES_PER_BLOCK;
			const uint32 currentBytes = currentCount * floatsPerVertex * floatSize;
			
			const uint64 baseOffset = (uint64)(batch * vertexCount + v) * floatsPerVertex * floatSize;
			const uint64 positionsEffAddr = g_Job.m_PositionsEffAddr + baseOffset;

			// Wait for previous DMA put for this vertex block
			if (blockIdx >= 2u)
			{
				waitForTag(1u << dataTag);
			}
			
			// DMA get current vertex block
			dmaGetVertices(bufIdx, positionsEffAddr, currentBytes, dataTag);
			
			// Prefetch next vertex block
			const uint32 nextVertex = v + MAX_VERTICES_PER_BLOCK;
			if (nextVertex < vertexCount)
			{
				const uint32 nextRemaining = vertexCount - nextVertex;
				const uint32 nextCount = (nextRemaining < MAX_VERTICES_PER_BLOCK) ? nextRemaining : MAX_VERTICES_PER_BLOCK;
				const uint32 nextBytes = nextCount * floatsPerVertex * floatSize;
				const uint64 nextBaseOffset = (uint64)(batch * vertexCount + nextVertex) * floatsPerVertex * floatSize;
				const uint64 nextPositionsEffAddr = g_Job.m_PositionsEffAddr + nextBaseOffset;
				
				// Wait for previous DMA put for next vertex block
				if (blockIdx >= 1u)
				{
					waitForTag(1u << nextDataTag);
				}
				
				// DMA get next vertex block
				dmaGetVertices(
					nextBufIdx,
					nextPositionsEffAddr,
					nextBytes,
					nextDataTag);
			}
			
			// Wait for current vertex block DMA get
			waitForTag(1u << dataTag);

			float32* pVertexBlock = g_PositionBuffer[bufIdx];
			
			// Transform vertices
			if (hasVertexW)
			{
				transformBlock4Component(pVertexBlock, currentCount, matrix);
			}
			else
			{
				transformBlock3Component(pVertexBlock, currentCount, matrix);
			}

			// DMA put transformed vertex block
			dmaPutVertices(bufIdx, positionsEffAddr, currentBytes, dataTag);
		}
		
		// Wait for vertex blocks DMA put
		waitForTag(1u << DATA_TAG0);
		waitForTag(1u << DATA_TAG1);
	}

	g_Job.m_Status = SPU_BATCH_STATUS_DONE;
}

int main(uint64 jobEffAddr, uint64 arg1, uint64 arg2, uint64 arg3)
{
	while (true)
	{
		spu_read_signal1();

		mfc_get(&g_Job, jobEffAddr, sizeof(SpuBatchJob_t), JOB_TAG, 0, 0);
		waitForTag(1u << JOB_TAG);

		if (g_Job.m_Command == SPU_BATCH_CMD_TERMINATE)
		{
			g_Job.m_Command = SPU_BATCH_CMD_IDLE;
			g_Job.m_Status = SPU_BATCH_STATUS_DONE;
			mfc_put(&g_Job, jobEffAddr, sizeof(SpuBatchJob_t), JOB_TAG, 0, 0);

			waitForTag(1u << JOB_TAG);
			
			break;
		}

		if (g_Job.m_Command == SPU_BATCH_CMD_TRANSFORM)
		{
			g_Job.m_Status = SPU_BATCH_STATUS_BUSY;
			processTransforms();
		}
		else
		{
			g_Job.m_Status = SPU_BATCH_STATUS_ERROR;
		}

		g_Job.m_Command = SPU_BATCH_CMD_IDLE;
		mfc_put(&g_Job, jobEffAddr, sizeof(SpuBatchJob_t), JOB_TAG, 0, 0);

		waitForTag(1u << JOB_TAG);
	}

	return 0;
}