//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "SpuFrustumCullManager.h"
#include "tier0/dbg.h"
#include "platform/ps3/spu/SpuUtils.h"
#include "utils/UtlMemory.h"
#include <string.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/event_queue.h>

extern "C"
{
	extern uint8_t _binary_SpuFrustumCullJob_elf_start[];
	extern uint8_t _binary_SpuFrustumCullJob_elf_size[];
}

CSpuFrustumCullManager::CSpuFrustumCullManager() :
	m_pJob(GCMGL_NULL),
	m_EventQueue(0),
	m_SpuThreadId(0),
	m_SpuGroupId(0),
	m_IsShuttingDown(false),
	m_HasPendingCull(false)
{
}

CSpuFrustumCullManager::~CSpuFrustumCullManager()
{
	Shutdown();
}

bool CSpuFrustumCullManager::Initialize()
{
	const int32 spuInitResult = sysSpuInitialize(6, 0);
	if (spuInitResult < 0)
	{
		return false;
	}

	const int32 spuImageImportResult = sysSpuImageImport(
		&m_SpuImage,
		_binary_SpuFrustumCullJob_elf_start,
		0);
	if (spuImageImportResult < 0)
	{
		return false;
	}

	sysSpuThreadGroupAttribute groupAttr = { 0 };
	groupAttr.name = const_cast<char*>("gcmCullGroup");

	const int32 spuThreadGroupCreateResult = sysSpuThreadGroupCreate(
		&m_SpuGroupId,
		1,
		100,
		&groupAttr);
	if (spuThreadGroupCreateResult < 0)
	{
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	m_pJob = static_cast<SpuFrustumCullJob_t*>(
		CUtlMemory::AlignedAlloc(sizeof(SpuFrustumCullJob_t), 128));
	if (!m_pJob)
	{
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	memset(m_pJob, 0, sizeof(SpuFrustumCullJob_t));
	m_pJob->m_Command = SPU_CULL_CMD_IDLE;
	m_pJob->m_Status = SPU_CULL_STATUS_IDLE;

	sysSpuThreadAttribute spuThreadAttribute = { 0 };
	spuThreadAttribute.name = const_cast<char*>("gcmCullSpu");
	spuThreadAttribute.option = SPU_THREAD_ATTR_NONE;

	sysSpuThreadArgument spuThreadArgument = { 0 };
	spuThreadArgument.arg0 = CSpuUtils::PtrToEa(m_pJob);

	const int32 spuThreadInitializeResult = sysSpuThreadInitialize(
		&m_SpuThreadId,
		m_SpuGroupId,
		0,
		&m_SpuImage,
		&spuThreadAttribute,
		&spuThreadArgument);
	if (spuThreadInitializeResult < 0)
	{
		CUtlMemory::AlignedFree(m_pJob);
		m_pJob = GCMGL_NULL;
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	sysSpuThreadSetConfiguration(
		m_SpuThreadId,
		SPU_SIGNAL1_OVERWRITE | SPU_SIGNAL2_OVERWRITE);

	sys_event_queue_attr_t eventQueueAttr;
	eventQueueAttr.attr_protocol = SYS_EVENT_QUEUE_FIFO;
	eventQueueAttr.type = SYS_EVENT_QUEUE_PPU;
	eventQueueAttr.name[0] = '\0';

	const int32 eventQueueCreateResult = sysEventQueueCreate(
		&m_EventQueue,
		&eventQueueAttr,
		SYS_EVENT_QUEUE_KEY_LOCAL,
		1);
	const int32 spuThreadConnectEventResult = (eventQueueCreateResult >= 0)
		? sysSpuThreadConnectEvent(
			m_SpuThreadId,
			m_EventQueue,
			SPU_THREAD_EVENT_USER,
			0)
		: -1;

	if (eventQueueCreateResult < 0 || spuThreadConnectEventResult < 0)
	{
		if (m_EventQueue)
		{
			sysEventQueueDestroy(m_EventQueue, 0);
			m_EventQueue = 0;
		}

		CUtlMemory::AlignedFree(m_pJob);
		m_pJob = GCMGL_NULL;
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	const int32 spuThreadGroupStartResult = sysSpuThreadGroupStart(m_SpuGroupId);
	if (spuThreadGroupStartResult < 0)
	{
		sysEventQueueDestroy(m_EventQueue, 0);
		m_EventQueue = 0;
		CUtlMemory::AlignedFree(m_pJob);
		m_pJob = GCMGL_NULL;
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	return true;
}

void CSpuFrustumCullManager::Shutdown()
{
	m_IsShuttingDown = true;

	if (m_SpuGroupId != 0)
	{
		if (m_pJob)
		{
			__sync_synchronize();

			if (m_pJob->m_Status != SPU_CULL_STATUS_IDLE &&
				m_pJob->m_Status != SPU_CULL_STATUS_DONE)
			{
				m_pJob->m_Command = SPU_CULL_CMD_TERMINATE;
				m_pJob->m_Status = SPU_CULL_STATUS_BUSY;

				__sync_synchronize();

				if (m_SpuThreadId != 0)
				{
					sysSpuThreadWriteSignal(m_SpuThreadId, 0, 1);
				}

				for (uint32 i = 0; i < 1000; i++)
				{
					__sync_synchronize();

					if (m_pJob->m_Status == SPU_CULL_STATUS_IDLE ||
						m_pJob->m_Status == SPU_CULL_STATUS_DONE)
					{
						break;
					}

					sysUsleep(1000); // 1ms
				}
			}
		}

		sysUsleep(10000); // 10ms

		sysSpuThreadGroupTerminate(m_SpuGroupId, 0);

		uint32 cause;
		uint32 status;
		sysSpuThreadGroupJoin(m_SpuGroupId, &cause, &status);
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		m_SpuGroupId = 0;
	}

	if (m_EventQueue)
	{
		sysEventQueueDestroy(m_EventQueue, 0);
		m_EventQueue = 0;
	}

	if (m_pJob)
	{
		CUtlMemory::AlignedFree(m_pJob);
		m_pJob = GCMGL_NULL;
	}

	m_SpuThreadId = 0;
	sysSpuImageClose(&m_SpuImage);
}

SPUResult_t::Enum CSpuFrustumCullManager::BeginCull(
	const BatchChunkTransform_t* pSrcTransforms,
	BatchChunkTransform_t* pDstTransforms,
	uint32* pDstCount,
	uint32 transformCount,
	const Plane_t* pPlanes)
{
	if (m_IsShuttingDown || !m_pJob || transformCount == 0)
	{
		return SPUResult_t::NotUsed;
	}

	for (uint32 i = 0; i < 6; i++)
	{
		m_pJob->m_Planes[i][0] = pPlanes[i].m_Normal.m_X;
		m_pJob->m_Planes[i][1] = pPlanes[i].m_Normal.m_Y;
		m_pJob->m_Planes[i][2] = pPlanes[i].m_Normal.m_Z;
		m_pJob->m_Planes[i][3] = pPlanes[i].m_Distance;
	}

	m_pJob->m_SrcTransformsEffAddr = CSpuUtils::PtrToEa(
		const_cast<BatchChunkTransform_t*>(pSrcTransforms));
	m_pJob->m_DstTransformsEffAddr = CSpuUtils::PtrToEa(pDstTransforms);
	m_pJob->m_DstCountEffAddr = CSpuUtils::PtrToEa(pDstCount);
	m_pJob->m_TransformCount = transformCount;
	m_pJob->m_TransformStride = 48u;
	m_pJob->m_Command = SPU_CULL_CMD_CULL;
	m_pJob->m_Status = SPU_CULL_STATUS_IDLE;

	__sync_synchronize();

	if (sysSpuThreadWriteSignal(m_SpuThreadId, 0, 1) < 0)
	{
		m_pJob->m_Command = SPU_CULL_CMD_IDLE;
		m_pJob->m_Status = SPU_CULL_STATUS_ERROR;

		return SPUResult_t::Error;
	}

	m_HasPendingCull = true;

	return SPUResult_t::Success;
}

SPUResult_t::Enum CSpuFrustumCullManager::WaitCull()
{
	if (!m_HasPendingCull)
	{
		return SPUResult_t::Success;
	}

	sys_event_t event;
	const bool hasError = sysEventQueueReceive(m_EventQueue, &event, 0) < 0 ||
		static_cast<uint32>(event.data_2) == SPU_CULL_STATUS_ERROR;

	m_pJob->m_Command = SPU_CULL_CMD_IDLE;
	m_pJob->m_Status = SPU_CULL_STATUS_IDLE;
	m_HasPendingCull = false;

	return hasError ? SPUResult_t::Error : SPUResult_t::Success;
}