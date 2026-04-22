//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "SpuBatchTransformManager.h"
#include "tier0/dbg.h"
#include "platform/ps3/spu/SpuUtils.h"
#include "utils/UtlMemory.h"
#include "renderer/Renderer.h"
#include "rsxutil/rsxutil.h"
#include <string.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/event_queue.h>

extern "C"
{
	extern uint8_t _binary_SpuBatchTransformJob_elf_start[];
	extern uint8_t _binary_SpuBatchTransformJob_elf_size[];
}

CSpuBatchTransformManager::CSpuBatchTransformManager()
{
	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		m_pBatchJobs[i] = GCMGL_NULL;
		m_SpuThreadIds[i] = 0;
		m_EventQueues[i] = 0;
	}
	m_SpuGroupId = 0;
	m_IsShuttingDown = false;
}

CSpuBatchTransformManager::~CSpuBatchTransformManager()
{
	Shutdown();
}

bool CSpuBatchTransformManager::Init()
{
	const int32 spuInitializeResult = sysSpuInitialize(6, 0);
	if (spuInitializeResult < 0)
	{
		return false;
	}

	const int32 spuImageImportResult = sysSpuImageImport(
		&m_SpuImage,
		_binary_SpuBatchTransformJob_elf_start,
		0);
	if (spuImageImportResult < 0)
	{
		return false;
	}

	sysSpuThreadGroupAttribute groupAttr = { 0 };
	groupAttr.name = const_cast<char*>("gcmBatchGroup");

	const int32 spuThreadGroupCreateResult = sysSpuThreadGroupCreate(
		&m_SpuGroupId,
		s_BatchTransformSpuCount,
		100,
		&groupAttr);
	if (spuThreadGroupCreateResult < 0)
	{
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		sysSpuThreadAttribute spuThreadAttribute = { 0 };
		spuThreadAttribute.name = const_cast<char*>("gcmBatchSpu");
		spuThreadAttribute.option = SPU_THREAD_ATTR_NONE;

		m_pBatchJobs[i] = static_cast<SpuBatchTransformJob_t*>(
			CUtlMemory::AlignedAlloc(sizeof(SpuBatchTransformJob_t), 128));
		if (!m_pBatchJobs[i])
		{
			sysSpuThreadGroupDestroy(m_SpuGroupId);
			sysSpuImageClose(&m_SpuImage);

			return false;
		}
		memset(m_pBatchJobs[i], 0, sizeof(SpuBatchTransformJob_t));

		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;

		sysSpuThreadArgument spuThreadArgument = { 0 };
		spuThreadArgument.arg0 = CSpuUtils::PtrToEa(m_pBatchJobs[i]);

		const int32 spuThreadInitializeResult = sysSpuThreadInitialize(
			&m_SpuThreadIds[i],
			m_SpuGroupId,
			i,
			&m_SpuImage,
			&spuThreadAttribute,
			&spuThreadArgument);
		if (spuThreadInitializeResult < 0)
		{
			for (uint32 j = 0; j <= i; j++)
			{
				if (m_pBatchJobs[j])
				{
					CUtlMemory::AlignedFree(m_pBatchJobs[j]);
					m_pBatchJobs[j] = GCMGL_NULL;
				}
			}

			sysSpuThreadGroupDestroy(m_SpuGroupId);
			sysSpuImageClose(&m_SpuImage);

			return false;
		}

		sysSpuThreadSetConfiguration(
			m_SpuThreadIds[i],
			SPU_SIGNAL1_OVERWRITE | SPU_SIGNAL2_OVERWRITE);

		sys_event_queue_attr_t eventQueueAttr;
		eventQueueAttr.attr_protocol = SYS_EVENT_QUEUE_FIFO;
		eventQueueAttr.type = SYS_EVENT_QUEUE_PPU;
		eventQueueAttr.name[0] = '\0';

		const int32 eventQueueCreateResult = sysEventQueueCreate(
			&m_EventQueues[i],
			&eventQueueAttr,
			SYS_EVENT_QUEUE_KEY_LOCAL,
			1);
		const int32 spuThreadConnectEventResult = (eventQueueCreateResult >= 0)
			? sysSpuThreadConnectEvent(
				m_SpuThreadIds[i],
				m_EventQueues[i],
				SPU_THREAD_EVENT_USER,
				0)
			: -1;

		if (eventQueueCreateResult < 0 || spuThreadConnectEventResult < 0)
		{
			for (uint32 j = 0; j <= i; j++)
			{
				if (m_EventQueues[j])
				{
					sysEventQueueDestroy(m_EventQueues[j], 0);
					m_EventQueues[j] = 0;
				}

				if (m_pBatchJobs[j])
				{
					CUtlMemory::AlignedFree(m_pBatchJobs[j]);
					m_pBatchJobs[j] = GCMGL_NULL;
				}
			}

			sysSpuThreadGroupDestroy(m_SpuGroupId);
			sysSpuImageClose(&m_SpuImage);

			return false;
		}
	}

	const int32 spuThreadGroupStartResult = sysSpuThreadGroupStart(
		m_SpuGroupId);
	if (spuThreadGroupStartResult < 0)
	{
		for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
		{
			if (m_EventQueues[i])
			{
				sysEventQueueDestroy(m_EventQueues[i], 0);
				m_EventQueues[i] = 0;
			}

			if (m_pBatchJobs[i])
			{
				CUtlMemory::AlignedFree(m_pBatchJobs[i]);
				m_pBatchJobs[i] = GCMGL_NULL;
			}
		}

		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	return true;
}

void CSpuBatchTransformManager::Shutdown()
{
	m_IsShuttingDown = true;

	if (m_SpuGroupId != 0)
	{
		for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
		{
			if (m_pBatchJobs[i])
			{
				__sync_synchronize();

				if (m_pBatchJobs[i]->m_Status != SPU_BATCH_STATUS_IDLE &&
					m_pBatchJobs[i]->m_Status != SPU_BATCH_STATUS_DONE)
				{
					m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_TERMINATE;
					m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_BUSY;

					__sync_synchronize();

					if (m_SpuThreadIds[i] != 0)
					{
						sysSpuThreadWriteSignal(m_SpuThreadIds[i], 0, 1);
					}
				}
			}
		}

		bool hasActiveJobs = true;
		for (uint32 i = 0; i < 1000; i++)
		{
			__sync_synchronize();

			hasActiveJobs = false;

			for (uint32 j = 0; j < s_BatchTransformSpuCount; j++)
			{
				if (m_pBatchJobs[j] &&
					m_pBatchJobs[j]->m_Status != SPU_BATCH_STATUS_IDLE &&
					m_pBatchJobs[j]->m_Status != SPU_BATCH_STATUS_DONE)
				{
					hasActiveJobs = true;

					break;
				}
			}

			if (!hasActiveJobs)
			{
				break;
			}

			sysUsleep(1000); // 1ms
		}

		sysUsleep(10000); // 10ms

		sysSpuThreadGroupTerminate(m_SpuGroupId, 0);

		uint32 cause;
		uint32 status;
		sysSpuThreadGroupJoin(m_SpuGroupId, &cause, &status);
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		m_SpuGroupId = 0;
	}

	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		if (m_pBatchJobs[i])
		{
			CUtlMemory::AlignedFree(m_pBatchJobs[i]);
			m_pBatchJobs[i] = GCMGL_NULL;
		}

		if (m_EventQueues[i])
		{
			sysEventQueueDestroy(m_EventQueues[i], 0);
			m_EventQueues[i] = 0;
		}

		m_SpuThreadIds[i] = 0;
	}

	sysSpuImageClose(&m_SpuImage);
}

SPUResult_t::Enum CSpuBatchTransformManager::BeginBatch(
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
	uint32 baseVertex)
{
	if (m_IsShuttingDown)
	{
		return SPUResult_t::NotUsed;
	}

	if (vertexCount == 0 || batchCount == 0)
	{
		return SPUResult_t::NotUsed;
	}

	if ((vertexStride % 16) != 0)
	{
		AssertMsg(
			0,
			"CSpuBatchTransformManager: Vertex stride is not a multiple of 16");

		return SPUResult_t::Error;
	}

	if ((indexCount % 4) != 0)
	{
		AssertMsg(
			0,
			"CSpuBatchTransformManager: Index count is not a multiple of 4");

		return SPUResult_t::Error;
	}
	
	if (((uintptr_t)pSrcVertices % 16) != 0)
	{
		AssertMsg(
			0,
			"CSpuBatchTransformManager: Source vertices are not 16-byte aligned");

		return SPUResult_t::Error;
	}

	if (((uintptr_t)pDstVertices % 16) != 0)
	{
		AssertMsg(
			0,
			"CSpuBatchTransformManager: Destination vertices are not 16-byte aligned");

		return SPUResult_t::Error;
	}

	uint32 batchIndex = 0;

	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		if (!m_pBatchJobs[i])
		{
			continue;
		}

		uint32 chunkBatches = (batchCount + (s_BatchTransformSpuCount - 1 - i)) / s_BatchTransformSpuCount;
		if (chunkBatches == 0)
		{
			continue;
		}

		uint64 offset = uint64(batchIndex) * vertexCount * vertexStride;
		
		SpuBatchTransformJob_t& spuBatchTransformJob = *m_pBatchJobs[i];
		spuBatchTransformJob.m_SrcVerticesEffAddr = CSpuUtils::PtrToEa(
			(void*)pSrcVertices);
		spuBatchTransformJob.m_SrcIndicesEffAddr = CSpuUtils::PtrToEa(
			(void*)pSrcIndices);
		spuBatchTransformJob.m_TransformsEffAddr = CSpuUtils::PtrToEa(
			(void*)(pTransforms + batchIndex));
		spuBatchTransformJob.m_DstVerticesEffAddr = CSpuUtils::PtrToEa(
			pDstVertices + offset);
		spuBatchTransformJob.m_DstIndicesEffAddr = CSpuUtils::PtrToEa(
			pDstIndices + (batchIndex * indexCount));
		spuBatchTransformJob.m_Command = SPU_BATCH_CMD_TRANSFORM;
		spuBatchTransformJob.m_Status = SPU_BATCH_STATUS_IDLE;
		spuBatchTransformJob.m_VertexCount = vertexCount;
		spuBatchTransformJob.m_IndexCount = indexCount;
		spuBatchTransformJob.m_BatchCount = chunkBatches;
		spuBatchTransformJob.m_VertexStride = vertexStride;
		spuBatchTransformJob.m_TransformStride = sizeof(BatchChunkTransform_t);
		spuBatchTransformJob.m_VertexPositionOffset = vertexPosOffset;
		spuBatchTransformJob.m_BaseVertex = baseVertex + (batchIndex * vertexCount);

		batchIndex += chunkBatches;
	}

	__sync_synchronize();

	bool hasError = false;

	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		if (m_pBatchJobs[i] && m_pBatchJobs[i]->m_Command == SPU_BATCH_CMD_TRANSFORM)
		{
			const int32 spuThreadWriteSignalResult = sysSpuThreadWriteSignal(
				m_SpuThreadIds[i],
				0,
				1);
			if (spuThreadWriteSignalResult < 0)
			{
				m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
				m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_ERROR;

				hasError = true;
			}
		}
	}

	return hasError ? SPUResult_t::Error : SPUResult_t::Success;
}

SPUResult_t::Enum CSpuBatchTransformManager::WaitBatch()
{
	bool hasError = false;

	for (uint32 i = 0; i < s_BatchTransformSpuCount; i++)
	{
		if (!m_pBatchJobs[i] || m_pBatchJobs[i]->m_Command == SPU_BATCH_CMD_IDLE)
		{
			continue;
		}

		sys_event_t event;
		if (sysEventQueueReceive(m_EventQueues[i], &event, 0) < 0)
		{
			hasError = true;
		}
		else if (static_cast<uint32>(event.data_2) == SPU_BATCH_STATUS_ERROR)
		{
			hasError = true;
		}

		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;
	}

	return hasError ? SPUResult_t::Error : SPUResult_t::Success;
}