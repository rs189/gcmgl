//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include <stdbool.h>
#include <string.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "SpuBatchTransformJob.h"
#include "SpuUtils.h"

#define JOB_TAG 0u
#define SRC_TAG0 1u
#define SRC_TAG1 6u
#define MATRIX_TAG0 2u
#define MATRIX_TAG1 3u
#define DST_TAG0 4u
#define DST_TAG1 5u

#define SPU_BUFFER_SIZE 16384u // 16 KB

static const vec_uchar16 s_SplatXPattern = { 0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3 };
static const vec_uchar16 s_SplatYPattern = { 4,5,6,7, 4,5,6,7, 4,5,6,7, 4,5,6,7 };
static const vec_uchar16 s_SplatZPattern = { 8,9,10,11, 8,9,10,11, 8,9,10,11, 8,9,10,11 };
static const vec_uchar16 s_SplatXYZW1Pattern = { 0,1,2,3, 4,5,6,7, 8,9,10,11, 28,29,30,31 };

static uint8 g_DstBuffer[2][SPU_BUFFER_SIZE] __attribute__((aligned(128)));
static uint8 g_SrcBuffer[2][SPU_BUFFER_SIZE] __attribute__((aligned(128)));
static vec_float4 g_MatrixBuffer[2][4] __attribute__((aligned(128)));
static SpuBatchJob_t g_Job __attribute__((aligned(128)));

// Flushes the current block and flips the buffer index
static inline void flushDstBlock(
	uint32* pBufferIndex,
	uint64* pEffAddr,
	uint32 blockBytes)
{
	uint32 tag = *pBufferIndex ? DST_TAG1 : DST_TAG0;
	dmaPut(g_DstBuffer[*pBufferIndex], *pEffAddr, blockBytes, tag);
	*pEffAddr += blockBytes;

	*pBufferIndex ^= 1u;
	waitForTag(1u << (*pBufferIndex ? DST_TAG1 : DST_TAG0));
}

// Aligned copy
static inline void copyVertex(
	uint8* pDstBytes,
	const uint8* pSrcBytes,
	uint32 sizeBytes)
{
	uint32 qwords = sizeBytes >> 4;

	vec_uint4* pDst = (vec_uint4*)pDstBytes;
	const vec_uint4* pSrc = (const vec_uint4*)pSrcBytes;

	switch (qwords)
	{
		case 1:
			pDst[0] = pSrc[0];

			break;
		case 2:
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[1];

			break;
		case 3:
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[1];
			pDst[2] = pSrc[2];

			break;
		case 4:
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[1];
			pDst[2] = pSrc[2];
			pDst[3] = pSrc[3];

			break;
		default:
			for (uint32 i = 0; i < qwords; ++i)
			{
				pDst[i] = pSrc[i];
			}

			break;
	}
}

static inline vec_float4 loadPosition(const uint8* p)
{
	uint32 offset = (uint32)((uintptr_t)p & 15u);

	const vec_uint4* pAligned = (const vec_uint4*)((uintptr_t)p & ~15u);
	const vec_float4 w1 = (vec_float4){ 0.0f, 0.0f, 0.0f, 1.0f };

	// Likely
	if (__builtin_expect(offset == 0, 1))
	{
		return spu_shuffle((vec_float4)pAligned[0], w1, s_SplatXYZW1Pattern);
	}

	// Aligned position
	vec_uint4 res = spu_or(
		spu_slqwbyte(pAligned[0], offset),
		spu_rlmaskqwbyte(pAligned[1], -(int)offset));

	return spu_shuffle((vec_float4)res, w1, s_SplatXYZW1Pattern);
}

static inline void storePosition(uint8* p, vec_float4 pos)
{
	uint32 offset = (uint32)((uintptr_t)p & 15u);

	vec_uint4* pAligned = (vec_uint4*)((uintptr_t)p & ~15u);
	vec_uint4 xyz = (vec_uint4){ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };

	// Likely
	if (__builtin_expect(offset == 0, 1))
	{
		pAligned[0] = spu_sel(pAligned[0], (vec_uint4)pos, xyz);
	}
	else if (offset == 4)
	{
		pAligned[0] = spu_sel(
			pAligned[0],
			(vec_uint4)spu_slqwbyte((vec_uint4)pos, 4),
			(vec_uint4)spu_slqwbyte(xyz, 4));
	}
	else
	{
		uint32 tail = 16 - offset;

		pAligned[0] = spu_sel(
			pAligned[0],
			(vec_uint4)spu_slqwbyte((vec_uint4)pos, offset),
			(vec_uint4)spu_slqwbyte(xyz, offset));
		pAligned[1] = spu_sel(
			pAligned[1],
			(vec_uint4)spu_rlmaskqwbyte((vec_uint4)pos, -(int)tail),
			(vec_uint4)spu_rlmaskqwbyte(xyz, -(int)tail));
	}
}

// Transforms a single vertex
static inline vec_float4 transformVertex(
	vec_float4 v,
	const vec_float4 pMatrix[4])
{
	vec_float4 result = spu_mul(pMatrix[0], spu_shuffle(v, v, s_SplatXPattern));
	result = spu_madd(pMatrix[1], spu_shuffle(v, v, s_SplatYPattern), result);
	result = spu_madd(pMatrix[2], spu_shuffle(v, v, s_SplatZPattern), result);

	return spu_add(result, pMatrix[3]);
}

// Transforms 4 vertices
static inline void transformVertices4(
	uint8* const pDst[4],
	const vec_float4 positions[4],
	const vec_float4 pMatrix[4],
	uint32 posOffset)
{
	vec_float4 splatX0 = spu_shuffle(
		positions[0],
		positions[0],
		s_SplatXPattern);
	vec_float4 splatX1 = spu_shuffle(
		positions[1],
		positions[1],
		s_SplatXPattern);
	vec_float4 splatY0 = spu_shuffle(
		positions[0],
		positions[0],
		s_SplatYPattern);
	vec_float4 result0 = spu_mul(pMatrix[0], splatX0);
	vec_float4 splatY1 = spu_shuffle(
		positions[1],
		positions[1],
		s_SplatYPattern);
	vec_float4 result1 = spu_mul(pMatrix[0], splatX1);
	vec_float4 splatX2 = spu_shuffle(
		positions[2],
		positions[2],
		s_SplatXPattern);
	vec_float4 splatX3 = spu_shuffle(
		positions[3],
		positions[3],
		s_SplatXPattern);
	vec_float4 result2 = spu_mul(pMatrix[0], splatX2);
	vec_float4 splatY2 = spu_shuffle(
		positions[2],
		positions[2],
		s_SplatYPattern);
	vec_float4 result3 = spu_mul(pMatrix[0], splatX3);
	vec_float4 splatY3 = spu_shuffle(
		positions[3],
		positions[3],
		s_SplatYPattern);

	vec_float4 splatZ0 = spu_shuffle(
		positions[0],
		positions[0],
		s_SplatZPattern);
	result0 = spu_madd(pMatrix[1], splatY0, result0);
	vec_float4 splatZ1 = spu_shuffle(
		positions[1],
		positions[1],
		s_SplatZPattern);
	result1 = spu_madd(pMatrix[1], splatY1, result1);
	vec_float4 splatZ2 = spu_shuffle(
		positions[2],
		positions[2],
		s_SplatZPattern);
	result2 = spu_madd(pMatrix[1], splatY2, result2);
	vec_float4 splatZ3 = spu_shuffle(
		positions[3],
		positions[3],
		s_SplatZPattern);
	result3 = spu_madd(pMatrix[1], splatY3, result3);

	result0 = spu_madd(pMatrix[2], splatZ0, result0);
	result1 = spu_madd(pMatrix[2], splatZ1, result1);
	result2 = spu_madd(pMatrix[2], splatZ2, result2);
	result3 = spu_madd(pMatrix[2], splatZ3, result3);

	vec_float4 t2 = spu_add(result2, pMatrix[3]);
	vec_float4 t3 = spu_add(result3, pMatrix[3]);
	vec_float4 t0 = spu_add(result0, pMatrix[3]);
	vec_float4 t1 = spu_add(result1, pMatrix[3]);

	storePosition(pDst[0] + posOffset, t0);
	storePosition(pDst[1] + posOffset, t1);
	storePosition(pDst[2] + posOffset, t2);
	storePosition(pDst[3] + posOffset, t3);
}

static void transformVertices()
{
	const uint32 vertexStride = g_Job.m_VertexStride;
	const uint32 posOffset = g_Job.m_VertexPositionOffset;
	const uint32 verticesPerBlock = SPU_BUFFER_SIZE / vertexStride;
	const uint32 blockBytes = verticesPerBlock * vertexStride;
	uint64 dstEffAddr = g_Job.m_DstVerticesEffAddr;
	uint32 dstBufferIndex = 0;
	uint32 dstVertexCount = 0;

	dmaGet(g_MatrixBuffer[0], g_Job.m_MatricesEffAddr, 64, MATRIX_TAG0);

	for (uint32 batchIndex = 0; batchIndex < g_Job.m_BatchCount; batchIndex++)
	{
		const uint32 matrixBufferIndex = batchIndex & 1u;
		const uint32 nextmatrixBufferIndex = matrixBufferIndex ^ 1u;
		const uint32 matrixTag = matrixBufferIndex ? MATRIX_TAG1 : MATRIX_TAG0;
		const uint32 nextMatrixTag = nextmatrixBufferIndex ? MATRIX_TAG1 : MATRIX_TAG0;

		// Prefetch
		if (batchIndex + 1u < g_Job.m_BatchCount)
		{
			uint64 nextMatrixEffAddr = g_Job.m_MatricesEffAddr + (batchIndex + 1u) * g_Job.m_MatrixStride;
			dmaGet(
				g_MatrixBuffer[nextmatrixBufferIndex],
				nextMatrixEffAddr,
				64,
				nextMatrixTag);
		}

		waitForTag(1u << matrixTag);

		const vec_float4* pMatrix = g_MatrixBuffer[matrixBufferIndex];
		const uint32 vertexCount = g_Job.m_VertexCount;
		uint64 srcEffAddr = g_Job.m_SrcVerticesEffAddr;
		uint32 srcBufferIndex = 0;
		uint32 srcVertexCount = 0;

		uint32 vertexCountsBuffer[2] = {
			(vertexCount < verticesPerBlock) ? vertexCount : verticesPerBlock,
			0
		};
		dmaGet(
			g_SrcBuffer[0],
			srcEffAddr,
			vertexCountsBuffer[0] * vertexStride,
			SRC_TAG0);
		srcEffAddr += vertexCountsBuffer[0] * vertexStride;

		uint32 loadedVerticesCount = vertexCountsBuffer[0];

		uint32 j = 0;
		while (j < vertexCount)
		{
			waitForTag(1u << (srcBufferIndex ? SRC_TAG1 : SRC_TAG0));

			// Prefetch
			if (srcVertexCount == 0 && loadedVerticesCount < vertexCount)
			{
				uint32 pendingCount = vertexCount - loadedVerticesCount;
				if (pendingCount > verticesPerBlock)
				{
					pendingCount = verticesPerBlock;
				}

				vertexCountsBuffer[srcBufferIndex ^ 1u] = pendingCount;
				uint32 pendingCountBytes = (pendingCount * vertexStride + 15u) & ~15u;
				uint32 nextSrcTag = (srcBufferIndex ^ 1u) ? SRC_TAG1 : SRC_TAG0;
				dmaGet(
					g_SrcBuffer[srcBufferIndex ^ 1u],
					srcEffAddr,
					pendingCountBytes,
					nextSrcTag);
				srcEffAddr += pendingCount * vertexStride;
				loadedVerticesCount += pendingCount;
			}

			uint32 srcRemaining = vertexCountsBuffer[srcBufferIndex] - srcVertexCount;
			uint32 dstRemaining = verticesPerBlock - dstVertexCount;
			uint32 chunkCount = srcRemaining < dstRemaining ? srcRemaining : dstRemaining;
			uint32 alignedCount = chunkCount & ~3u;
			for (uint32 i = 0; i < alignedCount; i += 4)
			{
				uint8* pSrc[4];
				uint8* pDst[4];
				vec_float4 positions[4];

				for (int j = 0; j < 4; j++)
				{
					pSrc[j] = g_SrcBuffer[srcBufferIndex] + (srcVertexCount + i + j) * vertexStride;
					pDst[j] = g_DstBuffer[dstBufferIndex] + (dstVertexCount + i + j) * vertexStride;
					copyVertex(pDst[j], pSrc[j], vertexStride);
					positions[j] = loadPosition(pSrc[j] + posOffset);
				}

				transformVertices4(pDst, positions, pMatrix, posOffset);
			}

			for (uint32 i = alignedCount; i < chunkCount; i++)
			{
				uint8* pSrc = g_SrcBuffer[srcBufferIndex] + (srcVertexCount + i) * vertexStride;
				uint8* pDst = g_DstBuffer[dstBufferIndex] + (dstVertexCount + i) * vertexStride;
				copyVertex(pDst, pSrc, vertexStride);

				vec_float4 pos = transformVertex(
					loadPosition(pSrc + posOffset),
					pMatrix);
				float32* pOut = (float32*)(pDst + posOffset);
				pOut[0] = spu_extract(pos, 0);
				pOut[1] = spu_extract(pos, 1);
				pOut[2] = spu_extract(pos, 2);
			}

			j += chunkCount;
			dstVertexCount += chunkCount;
			srcVertexCount += chunkCount;

			if (srcVertexCount == vertexCountsBuffer[srcBufferIndex])
			{
				srcVertexCount = 0;
				srcBufferIndex ^= 1u;
			}

			if (dstVertexCount == verticesPerBlock)
			{
				flushDstBlock(&dstBufferIndex, &dstEffAddr, blockBytes);
				dstVertexCount = 0;
			}
		}
	}

	if (dstVertexCount > 0)
	{
		uint32 dstTag = dstBufferIndex ? DST_TAG1 : DST_TAG0;
		waitForTag(1u << dstTag);

		uint32 alignedBytes = (dstVertexCount * vertexStride + 15u) & ~15u;
		dmaPut(g_DstBuffer[dstBufferIndex], dstEffAddr, alignedBytes, dstTag);
	}

	waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
}

static inline void processIndexChunk(
	uint32* pDst,
	const uint32* pSrc,
	uint32 chunkCount,
	vec_uint4 indexOffsets)
{
	const uint32 alignedCount = chunkCount & ~15u;
	for (uint32 i = 0; i < alignedCount; i += 16)
	{
		vec_uint4 r0 = spu_add(*(vec_uint4*)&pSrc[i + 0], indexOffsets);
		vec_uint4 r1 = spu_add(*(vec_uint4*)&pSrc[i + 4], indexOffsets);
		vec_uint4 r2 = spu_add(*(vec_uint4*)&pSrc[i + 8], indexOffsets);
		vec_uint4 r3 = spu_add(*(vec_uint4*)&pSrc[i + 12], indexOffsets);
		*(vec_uint4*)&pDst[i + 0] = r0;
		*(vec_uint4*)&pDst[i + 4] = r1;
		*(vec_uint4*)&pDst[i + 8] = r2;
		*(vec_uint4*)&pDst[i + 12] = r3;
	}

	for (uint32 i = alignedCount; i < (chunkCount & ~3u); i += 4)
	{
		*(vec_uint4*)&pDst[i] = spu_add(*(vec_uint4*)&pSrc[i], indexOffsets);
	}
}

static void processIndices()
{
	if (g_Job.m_IndexCount == 0)
	{
		return;
	}

	const uint32 indicesPerBlock = SPU_BUFFER_SIZE / 4u; // 4 bytes per index
	const uint32 blockBytes = SPU_BUFFER_SIZE;
	uint64 dstEffAddr = g_Job.m_DstIndicesEffAddr;
	uint32 dstBufferIndex = 0;
	uint32 dstIndexCount = 0;

	for (uint32 batchIndex = 0; batchIndex < g_Job.m_BatchCount; batchIndex++)
	{
		const uint32 indexOffset = (uint32)((uint64)g_Job.m_BaseVertex + (uint64)batchIndex * g_Job.m_VertexCount);
		const vec_uint4 indexOffsetSplat = spu_splats(indexOffset);
		const uint32 indexCount = g_Job.m_IndexCount;
		uint64 srcEffAddr = g_Job.m_SrcIndicesEffAddr;
		uint32 srcBufferIndex = 0;
		uint32 srcIndexCount = 0;

		uint32 indexCountsBuffer[2] = {
			(indexCount < indicesPerBlock) ? indexCount : indicesPerBlock,
			0
		};
		uint32 pendingCountBytes = (indexCountsBuffer[0] * 4u + 15u) & ~15u;
		dmaGet(g_SrcBuffer[0], srcEffAddr, pendingCountBytes, SRC_TAG0);
		srcEffAddr += indexCountsBuffer[0] * 4u;

		uint32 loadedIndexCount = indexCountsBuffer[0];

		uint32 j = 0;
		while (j < indexCount)
		{
			waitForTag(1u << (srcBufferIndex ? SRC_TAG1 : SRC_TAG0));

			// Prefetch
			if (srcIndexCount == 0 && loadedIndexCount < indexCount)
			{
				uint32 pendingCount = indexCount - loadedIndexCount;
				if (pendingCount > indicesPerBlock)
				{
					pendingCount = indicesPerBlock;
				}

				indexCountsBuffer[srcBufferIndex ^ 1u] = pendingCount;
				uint32 pendingCountBytes = (pendingCount * 4u + 15u) & ~15u;
				uint32 nextSrcTag = (srcBufferIndex ^ 1u) ? SRC_TAG1 : SRC_TAG0;
				dmaGet(
					g_SrcBuffer[srcBufferIndex ^ 1u],
					srcEffAddr,
					pendingCountBytes,
					nextSrcTag);
				srcEffAddr += pendingCount * 4u;
				loadedIndexCount += pendingCount;
			}

			uint32 srcRemaining = indexCountsBuffer[srcBufferIndex] - srcIndexCount;
			uint32 dstRemaining = indicesPerBlock - dstIndexCount;
			uint32 chunkCount = srcRemaining < dstRemaining ? srcRemaining : dstRemaining;

			processIndexChunk(
				(uint32*)g_DstBuffer[dstBufferIndex] + dstIndexCount,
				(uint32*)g_SrcBuffer[srcBufferIndex] + srcIndexCount,
				chunkCount,
				indexOffsetSplat);

			j += chunkCount;
			dstIndexCount += chunkCount;
			srcIndexCount += chunkCount;

			if (srcIndexCount == indexCountsBuffer[srcBufferIndex])
			{
				srcIndexCount = 0;
				srcBufferIndex ^= 1u;
			}

			if (dstIndexCount == indicesPerBlock)
			{
				flushDstBlock(&dstBufferIndex, &dstEffAddr, blockBytes);
				dstIndexCount = 0;
			}
		}
	}

	if (dstIndexCount > 0)
	{
		uint32 dstTag = dstBufferIndex ? DST_TAG1 : DST_TAG0;
		waitForTag(1u << dstTag);

		uint32 alignedBytes = (dstIndexCount * 4u + 15u) & ~15u;
		dmaPut(g_DstBuffer[dstBufferIndex], dstEffAddr, alignedBytes, dstTag);
	}

	waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
}

int main(uint64 jobEffAddr, uint64 arg1, uint64 arg2, uint64 arg3)
{
	while (true)
	{
		spu_read_signal1();

		mfc_get(
			&g_Job,
			jobEffAddr,
			sizeof(SpuBatchJob_t),
			JOB_TAG,
			0,
			0);
		waitForTag(1u << JOB_TAG);

		if (g_Job.m_Command == SPU_BATCH_CMD_TERMINATE)
		{
			g_Job.m_Command = SPU_BATCH_CMD_IDLE;
			g_Job.m_Status = SPU_BATCH_STATUS_DONE;
			mfc_put(
				&g_Job,
				jobEffAddr,
				sizeof(SpuBatchJob_t),
				JOB_TAG,
				0,
				0);
			waitForTag(1u << JOB_TAG);
			break;
		}

		if (g_Job.m_Command == SPU_BATCH_CMD_TRANSFORM)
		{
			g_Job.m_Status = SPU_BATCH_STATUS_BUSY;
			transformVertices();

			if (g_Job.m_Status != SPU_BATCH_STATUS_ERROR)
			{
				processIndices();
			}

			if (g_Job.m_Status != SPU_BATCH_STATUS_ERROR)
			{
				g_Job.m_Status = SPU_BATCH_STATUS_DONE;
			}
		}
		else
		{
			g_Job.m_Status = SPU_BATCH_STATUS_ERROR;
		}

		g_Job.m_Command = SPU_BATCH_CMD_IDLE;
		mfc_put(
			&g_Job,
			jobEffAddr,
			sizeof(SpuBatchJob_t),
			JOB_TAG,
			0,
			0);
		waitForTag(1u << JOB_TAG);
	}

	return 0;
}