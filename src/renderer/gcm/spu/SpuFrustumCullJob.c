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
#include "SpuFrustumCullJob.h"
#include "SpuUtils.h"

#define JOB_TAG 0u
#define SRC_TAG 1u
#define DST_TAG 2u
#define CNT_TAG 3u

#define TRANSFORM_SIZE 48u
#define TRANSFORMS_PER_BLOCK (16384u / TRANSFORM_SIZE)

static uint8 g_SrcBuffer[2][TRANSFORMS_PER_BLOCK * TRANSFORM_SIZE] ALIGN128;
static uint8 g_DstBuffer[TRANSFORMS_PER_BLOCK * TRANSFORM_SIZE] ALIGN128;
static uint32 g_OutCount ALIGN16;
static SpuFrustumCullJob_t g_Job ALIGN128;

static void cullTransforms()
{
	const uint32 transformCount = g_Job.m_TransformCount;
	const uint32 stride = g_Job.m_TransformStride;

	vec_float4 planes[6];
	for (uint32 i = 0; i < 6; i++)
	{
		planes[i] = (vec_float4){
			g_Job.m_Planes[i][0],
			g_Job.m_Planes[i][1],
			g_Job.m_Planes[i][2],
			g_Job.m_Planes[i][3]
		};
	}

	const vec_float4 zero = spu_splats(0.0f);
	const vec_float4 half = spu_splats(0.5f);

	uint32 dstCount = 0;
	uint32 srcBufferIndex = 0;
	uint32 loaded = 0;

	const uint32 firstBlock = transformCount < TRANSFORMS_PER_BLOCK
		? transformCount
		: TRANSFORMS_PER_BLOCK;
	dmaGet(
		g_SrcBuffer[0],
		g_Job.m_SrcTransformsEffAddr,
		firstBlock * stride,
		SRC_TAG);

	loaded = firstBlock;

	uint64 dstEffAddr = g_Job.m_DstTransformsEffAddr;
	uint32 dstBufferCount = 0;

	for (uint32 transformIndex = 0; transformIndex < transformCount; transformIndex++)
	{
		const uint32 localIndex = transformIndex % TRANSFORMS_PER_BLOCK;

		if (localIndex == 0 && transformIndex > 0)
		{
			if (dstBufferCount > 0)
			{
				waitForTag(1u << DST_TAG);

				dmaPut(
					g_DstBuffer,
					dstEffAddr,
					dstBufferCount * stride,
					DST_TAG);
				dstEffAddr += dstBufferCount * stride;
				dstBufferCount = 0;
			}

			srcBufferIndex ^= 1u;

			if (loaded < transformCount)
			{
				const uint32 remaining = transformCount - loaded;
				const uint32 nextBlock = remaining < TRANSFORMS_PER_BLOCK
					? remaining
					: TRANSFORMS_PER_BLOCK;
				dmaGet(
					g_SrcBuffer[srcBufferIndex],
					g_Job.m_SrcTransformsEffAddr + loaded * stride,
					nextBlock * stride,
					SRC_TAG);
				loaded += nextBlock;
			}
		}

		waitForTag(1u << SRC_TAG);

		const uint8* pSrc = g_SrcBuffer[srcBufferIndex] + localIndex * stride;
		const float32* pPos = (const float32*)(pSrc + 16);
		const float32* pScale = (const float32*)(pSrc + 28);

		const vec_float4 center = (vec_float4){
			pPos[0],
			pPos[1],
			pPos[2],
			1.0f
		};
		const vec_float4 scaleVec = (vec_float4){
			pScale[0],
			pScale[1],
			pScale[2],
			0.0f
		};
		const vec_float4 extent = spu_mul(scaleVec, half);

		bool visible = true;
		for (uint32 i = 0; i < 6; i++)
		{
			const vec_float4 absPlane = (vec_float4){
				__builtin_fabsf(spu_extract(planes[i], 0)),
				__builtin_fabsf(spu_extract(planes[i], 1)),
				__builtin_fabsf(spu_extract(planes[i], 2)),
				0.0f
			};
			const vec_float4 r4 = spu_mul(absPlane, extent);
			const float32 radius =
				spu_extract(r4, 0) +
				spu_extract(r4, 1) +
				spu_extract(r4, 2);
			const vec_float4 d4 = spu_mul(planes[i], center);
			const float32 dist =
				spu_extract(d4, 0) +
				spu_extract(d4, 1) +
				spu_extract(d4, 2) +
				spu_extract(d4, 3);

			if (dist + radius < 0.0f)
			{
				visible = false;

				break;
			}
		}

		if (visible)
		{
			memcpy(g_DstBuffer + dstBufferCount * stride, pSrc, stride);

			dstBufferCount++;
			dstCount++;
		}
	}

	if (dstBufferCount > 0)
	{
		waitForTag(1u << DST_TAG);

		dmaPut(g_DstBuffer, dstEffAddr, dstBufferCount * stride, DST_TAG);
	}

	waitForTag(1u << DST_TAG);

	g_OutCount = dstCount;

	waitForTag(1u << CNT_TAG);

	dmaPut(&g_OutCount, g_Job.m_DstCountEffAddr, 16, CNT_TAG);

	waitForTag(1u << CNT_TAG);
}

int main(uint64 jobEffAddr, uint64 arg1, uint64 arg2, uint64 arg3)
{
	while (true)
	{
		spu_read_signal1();

		mfc_get(
			&g_Job,
			jobEffAddr,
			sizeof(SpuFrustumCullJob_t),
			JOB_TAG,
			0,
			0);

		waitForTag(1u << JOB_TAG);

		if (g_Job.m_Command == SPU_CULL_CMD_TERMINATE)
		{
			g_Job.m_Command = SPU_CULL_CMD_IDLE;
			g_Job.m_Status = SPU_CULL_STATUS_DONE;
			mfc_put(
				&g_Job,
				jobEffAddr,
				sizeof(SpuFrustumCullJob_t),
				JOB_TAG,
				0,
				0);

			waitForTag(1u << JOB_TAG);

			break;
		}

		if (g_Job.m_Command == SPU_CULL_CMD_CULL)
		{
			g_Job.m_Status = SPU_CULL_STATUS_BUSY;
			cullTransforms();
			g_Job.m_Status = SPU_CULL_STATUS_DONE;
		}
		else
		{
			g_Job.m_Status = SPU_CULL_STATUS_ERROR;
		}

		g_Job.m_Command = SPU_CULL_CMD_IDLE;
		mfc_put(
			&g_Job,
			jobEffAddr,
			sizeof(SpuFrustumCullJob_t),
			JOB_TAG,
			0,
			0);

		waitForTag(1u << JOB_TAG);

		spu_thread_send_event(0, g_Job.m_Status, 0);
	}

	return 0;
}