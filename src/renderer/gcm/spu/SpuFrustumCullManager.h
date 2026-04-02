//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_FRUSTUM_CULL_MANAGER_H
#define SPU_FRUSTUM_CULL_MANAGER_H

#pragma once

#include <sys/spu.h>
#include <sys/event_queue.h>
#include "platform/ps3/spu/SpuCommon.h"
#include "SpuFrustumCullJob.h"
#include "renderer/Renderer.h"

class CSpuFrustumCullManager
{
public:
	CSpuFrustumCullManager();
	~CSpuFrustumCullManager();

	bool Initialize();
	void Shutdown();

	SPUResult_t::Enum BeginCull(
		const BatchChunkTransform_t* pSrcTransforms,
		BatchChunkTransform_t* pDstTransforms,
		uint32* pDstCount,
		uint32 transformCount,
		const Plane_t* pPlanes);
	SPUResult_t::Enum WaitCull();
private:
	sysSpuImage m_SpuImage;
	SpuFrustumCullJob_t* m_pJob;
	sys_event_queue_t m_EventQueue;
	uint32 m_SpuThreadId;
	uint32 m_SpuGroupId;
	bool m_IsShuttingDown;
	bool m_HasPendingCull;
};

#endif // SPU_FRUSTUM_CULL_MANAGER_H