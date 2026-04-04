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
#include <sys/spu_event.h>
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
static const vec_uchar16 s_SplatWPattern = { 12,13,14,15, 12,13,14,15, 12,13,14,15, 12,13,14,15 };
static const vec_uchar16 s_SplatXYZW1Pattern = { 0,1,2,3, 4,5,6,7, 8,9,10,11, 28,29,30,31 };

static uint8 g_DstBuffer[2][SPU_BUFFER_SIZE] ALIGN128;
static uint8 g_SrcBuffer[2][SPU_BUFFER_SIZE] ALIGN128;
static uint8 g_IndexSrcBuffer[2][SPU_BUFFER_SIZE] ALIGN128;
static vec_float4 g_TransformBuffer[2][3] ALIGN128;
static SpuBatchTransformJob_t g_Job ALIGN128;

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

	if (LIKELY(offset == 0))
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

	if (LIKELY(offset == 0))
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

// Builds CMatrix4 as array of 4 vec_float4
static inline void transformFromBatchTransform(
	vec_float4 pMatrix[4],
	const vec_float4 buf[3])
{
	// buf[0] = (qx, qy, qz, qw)
	// buf[1] = (px, py, pz, sx)
	// buf[2] = (sy, sz,  _,  _)
	const vec_float4 q = buf[0];
	const vec_float4 q2 = spu_add(q, q);
	const vec_float4 one = spu_splats(1.0f);

	const vec_float4 qx = spu_shuffle(q, q, s_SplatXPattern);
	const vec_float4 qy = spu_shuffle(q, q, s_SplatYPattern);
	const vec_float4 qz = spu_shuffle(q, q, s_SplatZPattern);
	const vec_float4 qw = spu_shuffle(q, q, s_SplatWPattern);
	const vec_float4 q2x = spu_shuffle(q2, q2, s_SplatXPattern);
	const vec_float4 q2y = spu_shuffle(q2, q2, s_SplatYPattern);
	const vec_float4 q2z = spu_shuffle(q2, q2, s_SplatZPattern);

	const vec_float4 xx2 = spu_mul(qx, q2x);
	const vec_float4 yy2 = spu_mul(qy, q2y);
	const vec_float4 zz2 = spu_mul(qz, q2z);
	const vec_float4 xy2 = spu_mul(qx, q2y);
	const vec_float4 xz2 = spu_mul(qx, q2z);
	const vec_float4 yz2 = spu_mul(qy, q2z);
	const vec_float4 wx2 = spu_mul(qw, q2x);
	const vec_float4 wy2 = spu_mul(qw, q2y);
	const vec_float4 wz2 = spu_mul(qw, q2z);

	const float32 sx = spu_extract(buf[1], 3);
	const float32 sy = spu_extract(buf[2], 0);
	const float32 sz = spu_extract(buf[2], 1);

	vec_float4 col0 = {
		spu_extract(spu_sub(spu_sub(one, yy2), zz2), 0),
		spu_extract(spu_add(xy2, wz2), 0),
		spu_extract(spu_sub(xz2, wy2), 0),
		0.0f
	};
	vec_float4 col1 = {
		spu_extract(spu_sub(xy2, wz2), 0),
		spu_extract(spu_sub(spu_sub(one, xx2), zz2), 0),
		spu_extract(spu_add(yz2, wx2), 0),
		0.0f
	};
	vec_float4 col2 = {
		spu_extract(spu_add(xz2, wy2), 0),
		spu_extract(spu_sub(yz2, wx2), 0),
		spu_extract(spu_sub(spu_sub(one, xx2), yy2), 0),
		0.0f
	};

	pMatrix[0] = spu_mul(col0, spu_splats(sx));
	pMatrix[1] = spu_mul(col1, spu_splats(sy));
	pMatrix[2] = spu_mul(col2, spu_splats(sz));
	pMatrix[3] = (vec_float4){
		spu_extract(buf[1], 0),
		spu_extract(buf[1], 1),
		spu_extract(buf[1], 2),
		1.0f
	};
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

	dmaGet(g_TransformBuffer[0], g_Job.m_TransformsEffAddr, 48, MATRIX_TAG0);

	for (uint32 batchIndex = 0; batchIndex < g_Job.m_BatchCount; batchIndex++)
	{
		const uint32 transformBufferIndex = batchIndex & 1u;
		const uint32 nextTransformBufferIndex = transformBufferIndex ^ 1u;
		const uint32 transformTag = transformBufferIndex ? MATRIX_TAG1 : MATRIX_TAG0;
		const uint32 nextTransformTag = nextTransformBufferIndex ? MATRIX_TAG1 : MATRIX_TAG0;

		// Prefetch
		if (batchIndex + 1u < g_Job.m_BatchCount)
		{
			uint64 nextTransformEffAddr = g_Job.m_TransformsEffAddr + (batchIndex + 1u) * g_Job.m_TransformStride;
			dmaGet(
				g_TransformBuffer[nextTransformBufferIndex],
				nextTransformEffAddr,
				48,
				nextTransformTag);
		}

		waitForTag(1u << transformTag);

		vec_float4 matrix[4];
		transformFromBatchTransform(
			matrix,
			g_TransformBuffer[transformBufferIndex]);
		const vec_float4* pMatrix = matrix;
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

				for (uint32 k = 0; k < 4; k++)
				{
					pSrc[k] = g_SrcBuffer[srcBufferIndex] + (srcVertexCount + i + k) * vertexStride;
					pDst[k] = g_DstBuffer[dstBufferIndex] + (dstVertexCount + i + k) * vertexStride;
					copyVertex(pDst[k], pSrc[k], vertexStride);
					positions[k] = loadPosition(pSrc[k] + posOffset);
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

	for (uint32 i = (chunkCount & ~3u); i < chunkCount; i++)
	{
		pDst[i] = pSrc[i] + spu_extract(indexOffsets, 0);
	}
}

static void processIndices()
{
	if (g_Job.m_IndexCount == 0)
	{
		return;
	}

	const uint32 indexCount = g_Job.m_IndexCount;
	const uint32 indicesPerBlock = SPU_BUFFER_SIZE / 4u;

	uint32 loadCount = indexCount < indicesPerBlock ? indexCount : indicesPerBlock;
	uint32 loadBytes = (loadCount * 4u + 15u) & ~15u;
	dmaGet(g_IndexSrcBuffer[0], g_Job.m_SrcIndicesEffAddr, loadBytes, SRC_TAG0);

	waitForTag(1u << SRC_TAG0);

	uint64 dstEffAddr = g_Job.m_DstIndicesEffAddr;
	uint32 dstBufferIndex = 0;
	uint32 dstIndexCount = 0;

	for (uint32 batchIndex = 0; batchIndex < g_Job.m_BatchCount; batchIndex++)
	{
		const uint32 indexOffset = (uint32)((uint64)g_Job.m_BaseVertex + (uint64)batchIndex * g_Job.m_VertexCount);
		const vec_uint4 indexOffsetSplat = spu_splats(indexOffset);

		uint32 j = 0;
		while (j < indexCount)
		{
			uint32 dstRemaining = indicesPerBlock - dstIndexCount;
			uint32 srcRemaining = indexCount - j;
			uint32 chunkCount = srcRemaining < dstRemaining ? srcRemaining : dstRemaining;

			processIndexChunk(
				(uint32*)g_DstBuffer[dstBufferIndex] + dstIndexCount,
				(uint32*)g_IndexSrcBuffer[0] + j,
				chunkCount,
				indexOffsetSplat);

			j += chunkCount;
			dstIndexCount += chunkCount;

			if (dstIndexCount == indicesPerBlock)
			{
				flushDstBlock(&dstBufferIndex, &dstEffAddr, SPU_BUFFER_SIZE);
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
			sizeof(SpuBatchTransformJob_t),
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
				sizeof(SpuBatchTransformJob_t),
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
			sizeof(SpuBatchTransformJob_t),
			JOB_TAG,
			0,
			0);

		waitForTag(1u << JOB_TAG);

		spu_thread_send_event(0, g_Job.m_Status, 0);
	}

	return 0;
}