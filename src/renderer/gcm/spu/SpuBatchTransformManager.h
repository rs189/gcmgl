//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_BATCH_TRANSFORM_MANAGER_H
#define SPU_BATCH_TRANSFORM_MANAGER_H

#pragma once

#include <sys/spu.h>
#include <sys/event_queue.h>
#include "platform/ps3/spu/SpuCommon.h"
#include "SpuBatchTransformJob.h"

struct BatchChunkTransform_t;

class CSpuBatchTransformManager
{
public:
	CSpuBatchTransformManager();
	~CSpuBatchTransformManager();

	bool Initialize();
	void Shutdown();

	SPUResult_t::Enum BeginBatch(
		const char* pSrcVertices,
		const uint32* pSrcIndices,
		const BatchChunkTransform_t* pTransforms,
		char* pDstVertices,
		uint32* pDstIndices,
		uint32 vertexCount,
		uint32 indexCount,
		uint32 batchCount,
		uint32 vertexStride,
		uint32 vertexPosOffset,
		uint32 baseVertex);
	SPUResult_t::Enum WaitBatch();
private:
	static const uint32 s_BatchTransformSpuCount = 2;
	sysSpuImage m_SpuImage;
	SpuBatchTransformJob_t* m_pBatchJobs[s_BatchTransformSpuCount];
	sys_event_queue_t m_EventQueues[s_BatchTransformSpuCount];
	uint32 m_SpuThreadIds[s_BatchTransformSpuCount];
	uint32 m_SpuGroupId;
	bool m_IsShuttingDown;
};

#endif // SPU_BATCH_TRANSFORM_MANAGER_H