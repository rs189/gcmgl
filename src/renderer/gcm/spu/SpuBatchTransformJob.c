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

#define JOB_TAG 0u
#define SRC_TAG 1u
#define MATRIX_TAG0 2u
#define MATRIX_TAG1 3u
#define DST_TAG0 4u
#define DST_TAG1 5u

#define SPU_BUFFER_SIZE 16384u

static const vec_uchar16 SplatXPattern = {
	0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3
};
static const vec_uchar16 SplatYPattern = {
	4,5,6,7, 4,5,6,7, 4,5,6,7, 4,5,6,7
};
static const vec_uchar16 SplatZPattern = {
	8,9,10,11, 8,9,10,11, 8,9,10,11, 8,9,10,11
};

static SpuBatchJob_t g_Job __attribute__((aligned(128)));
static vec_float4 g_MatrixBuffer[2][4] __attribute__((aligned(128)));
static uint8_t g_SrcVertexBuffer[SPU_BUFFER_SIZE] __attribute__((aligned(128)));
static uint8_t g_SrcIndexBuffer[SPU_BUFFER_SIZE] __attribute__((aligned(128)));
static uint8_t g_DstVertexBuffer[2][SPU_BUFFER_SIZE] __attribute__((aligned(128)));
static uint8_t g_DstIndexBuffer[2][SPU_BUFFER_SIZE] __attribute__((aligned(128)));

static inline void waitForTag(uint32 mask)
{
	mfc_write_tag_mask(mask);
	spu_mfcstat(MFC_TAG_UPDATE_ALL);
}

static inline void dmaGet(void* ls, uint64 ea, uint32 size, uint32 tag)
{
	if (size == 0) return;

	mfc_get(ls, ea, size, tag, 0, 0);
}

static inline void dmaPut(void* ls, uint64 ea, uint32 size, uint32 tag)
{
	if (size == 0) return;

	mfc_put(ls, ea, size, tag, 0, 0);
}

static uint32 gcd(uint32 a, uint32 b)
{
	while (b)
	{
		uint32 t = b;
		b = a % b;
		a = t;
	}

	return a;
}

static uint32 lcm(uint32 a, uint32 b)
{
	if (a == 0 || b == 0) return 0;
	return (a * b) / gcd(a, b);
}

static inline vec_float4 transformVertex(vec_float4 v, const vec_float4 pMatrix[4])
{
	vec_float4 x = spu_shuffle(v, v, SplatXPattern);
	vec_float4 y = spu_shuffle(v, v, SplatYPattern);
	vec_float4 z = spu_shuffle(v, v, SplatZPattern);

	vec_float4 result = spu_mul(pMatrix[0], x);
	result = spu_madd(pMatrix[1], y, result);
	result = spu_madd(pMatrix[2], z, result);
	result = spu_add(result, pMatrix[3]);

	return result;
}

static void processVertices()
{
	uint32 strideBytesLCM = lcm(16u, g_Job.m_VertexStride);
	uint32 blockBytes = (SPU_BUFFER_SIZE / strideBytesLCM) * strideBytesLCM;
	
	uint32 srcVertexBytes = (g_Job.m_VertexCount * g_Job.m_VertexStride + 15u) & ~15u;
	if (srcVertexBytes > SPU_BUFFER_SIZE) {
		g_Job.m_Status = SPU_BATCH_STATUS_ERROR;

		return;
	}
	
	dmaGet(g_SrcVertexBuffer, g_Job.m_SrcVerticesEffAddr, srcVertexBytes, SRC_TAG);
	waitForTag(1u << SRC_TAG);

	uint32 outBufIdx = 0;
	uint32 outOffset = 0;
	uint64 outputEffAddr = g_Job.m_DstVerticesEffAddr;
	
	dmaGet(g_MatrixBuffer[0], g_Job.m_MatricesEffAddr, 64, MATRIX_TAG0);
	
	for (uint32 batch = 0; batch < g_Job.m_BatchCount; batch++)
	{
		const uint32 mBufIdx = batch & 1u;
		const uint32 nextMBufIdx = (batch + 1u) & 1u;
		const uint32 mTag = mBufIdx ? MATRIX_TAG1 : MATRIX_TAG0;
		const uint32 nextMTag = nextMBufIdx ? MATRIX_TAG1 : MATRIX_TAG0;

		if (batch + 1u < g_Job.m_BatchCount) {
			uint64 nextMatrixEa = g_Job.m_MatricesEffAddr + (batch + 1u) * g_Job.m_MatrixStride;
			dmaGet(g_MatrixBuffer[nextMBufIdx], nextMatrixEa, 64, nextMTag);
		}
		
		waitForTag(1u << mTag);
		const vec_float4* matrix = g_MatrixBuffer[mBufIdx];
		
		for (uint32 v = 0; v < g_Job.m_VertexCount; v++)
		{
			uint8_t* pSrc = g_SrcVertexBuffer + v * g_Job.m_VertexStride;
			uint8_t* pDst = g_DstVertexBuffer[outBufIdx] + outOffset;
			
			memcpy(pDst, pSrc, g_Job.m_VertexStride);
			
			float tmp[3];
			memcpy(tmp, pDst + g_Job.m_VertexPosOffset, 12);
			vec_float4 pos = { tmp[0], tmp[1], tmp[2], 1.0f };
			vec_float4 transformed = transformVertex(pos, matrix);
			tmp[0] = spu_extract(transformed, 0);
			tmp[1] = spu_extract(transformed, 1);
			tmp[2] = spu_extract(transformed, 2);
			memcpy(pDst + g_Job.m_VertexPosOffset, tmp, 12);
			
			outOffset += g_Job.m_VertexStride;
			
			if (outOffset == blockBytes) {
				uint32 dstTag = outBufIdx ? DST_TAG1 : DST_TAG0;
				waitForTag(1u << dstTag);
				
				dmaPut(g_DstVertexBuffer[outBufIdx], outputEffAddr, blockBytes, dstTag);
				outputEffAddr += blockBytes;
				
				outBufIdx ^= 1;
				outOffset = 0;
			}
		}
	}
	
	if (outOffset > 0) {
		uint32 dstTag = outBufIdx ? DST_TAG1 : DST_TAG0;
		waitForTag(1u << dstTag);
		
		uint32 finalPutSize = (outOffset + 15u) & ~15u;
		dmaPut(g_DstVertexBuffer[outBufIdx], outputEffAddr, finalPutSize, dstTag);
		waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
	} else {
		waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
	}
}

static void processIndices()
{
	if (g_Job.m_IndexCount == 0) return;
	
	uint32 blockBytes = SPU_BUFFER_SIZE;
	uint32 indicesPerBlock = blockBytes / 4u;
	
	uint32 srcIndexBytes = (g_Job.m_IndexCount * 4u + 15u) & ~15u;
	if (srcIndexBytes > SPU_BUFFER_SIZE) {
		g_Job.m_Status = SPU_BATCH_STATUS_ERROR;

		return;
	}
	
	dmaGet(g_SrcIndexBuffer, g_Job.m_SrcIndicesEffAddr, srcIndexBytes, SRC_TAG);

	waitForTag(1u << SRC_TAG);
	
	uint32* pSrcIndices = (uint32*)g_SrcIndexBuffer;
	
	uint32 outBufIdx = 0;
	uint32 outIdxCount = 0;
	uint64 outputEffAddr = g_Job.m_DstIndicesEffAddr;
	
	for (uint32 batch = 0; batch < g_Job.m_BatchCount; batch++)
	{
		uint32 indexOffset = g_Job.m_BaseVertex + (batch * g_Job.m_VertexCount);
		
		for (uint32 i = 0; i < g_Job.m_IndexCount; i++)
		{
			uint32* pDstIndices = (uint32*)g_DstIndexBuffer[outBufIdx];
			pDstIndices[outIdxCount] = pSrcIndices[i] + indexOffset;
			
			outIdxCount++;
			
			if (outIdxCount == indicesPerBlock) {
				uint32 dstTag = outBufIdx ? DST_TAG1 : DST_TAG0;
				waitForTag(1u << dstTag);
				
				dmaPut(
					g_DstIndexBuffer[outBufIdx],
					outputEffAddr,
					blockBytes,
					dstTag);
				outputEffAddr += blockBytes;
				
				outBufIdx ^= 1;
				outIdxCount = 0;
			}
		}
	}
	
	if (outIdxCount > 0) {
		uint32 dstTag = outBufIdx ? DST_TAG1 : DST_TAG0;
		waitForTag(1u << dstTag);
		
		uint32 finalBytes = outIdxCount * 4u;
		uint32 finalPutSize = (finalBytes + 15u) & ~15u;
		dmaPut(
			g_DstIndexBuffer[outBufIdx],
			outputEffAddr,
			finalPutSize,
			dstTag);

		waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
	} else {
		waitForTag((1u << DST_TAG0) | (1u << DST_TAG1));
	}
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
			processVertices();
			if (g_Job.m_Status != SPU_BATCH_STATUS_ERROR) {
				processIndices();
			}
			
			if (g_Job.m_Status != SPU_BATCH_STATUS_ERROR) {
				g_Job.m_Status = SPU_BATCH_STATUS_DONE;
			}
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