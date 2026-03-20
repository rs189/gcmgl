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
#include "mathsfury/Matrix4.h"
#include "rsxutil/rsxutil.h"
#include <string.h>
#include <sys/thread.h>
#include <sys/systime.h>

extern "C"
{
	extern uint8_t _binary_SpuBatchTransformJob_elf_start[];
	extern uint8_t _binary_SpuBatchTransformJob_elf_size[];
}

CSpuBatchTransformManager::CSpuBatchTransformManager() :
	m_SpuGroupId(0),
	m_SpuThreadId(0),
	m_pBatchJob(GCMGL_NULL),
	m_IsShuttingDown(false)
{
}

CSpuBatchTransformManager::~CSpuBatchTransformManager()
{
	Shutdown();
}

bool CSpuBatchTransformManager::Initialize()
{
	const int32 spuResult = sysSpuInitialize(6, 0);
	if (spuResult < 0)
	{
		return false;
	}

	const int32 importResult = sysSpuImageImport(
		&m_SpuImage,
		_binary_SpuBatchTransformJob_elf_start,
		0);
	if (importResult < 0)
	{
		return false;
	}

	sysSpuThreadGroupAttribute groupAttr = { 0 };
	groupAttr.name = const_cast<char*>("gcmBatchGroup");

	const int32 groupCreateResult = sysSpuThreadGroupCreate(
		&m_SpuGroupId,
		1,
		100,
		&groupAttr);
	if (groupCreateResult < 0)
	{
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	sysSpuThreadAttribute threadAttr = { 0 };
	threadAttr.name = const_cast<char*>("gcmBatchSpu");
	threadAttr.option = SPU_THREAD_ATTR_NONE;

	m_pBatchJob = static_cast<SpuBatchJob_t*>(
		CUtlMemory::AlignedAlloc(sizeof(SpuBatchJob_t), 128));
	if (!m_pBatchJob)
	{
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}
	memset(m_pBatchJob, 0, sizeof(SpuBatchJob_t));

	m_pBatchJob->m_Command = SPU_BATCH_CMD_IDLE;
	m_pBatchJob->m_Status = SPU_BATCH_STATUS_IDLE;

	sysSpuThreadArgument threadArg = { 0 };
	threadArg.arg0 = SpuUtils::PtrToEa(m_pBatchJob);

	const int32 threadInitResult = sysSpuThreadInitialize(
		&m_SpuThreadId,
		m_SpuGroupId,
		0,
		&m_SpuImage,
		&threadAttr,
		&threadArg);
	if (threadInitResult < 0)
	{
		CUtlMemory::AlignedFree(m_pBatchJob);
		m_pBatchJob = GCMGL_NULL;
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	sysSpuThreadSetConfiguration(
		m_SpuThreadId,
		SPU_SIGNAL1_OVERWRITE | SPU_SIGNAL2_OVERWRITE);

	const int32 startResult = sysSpuThreadGroupStart(m_SpuGroupId);
	if (startResult < 0)
	{
		CUtlMemory::AlignedFree(m_pBatchJob);
		m_pBatchJob = GCMGL_NULL;
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	return true;
}

void CSpuBatchTransformManager::Shutdown()
{
	m_IsShuttingDown = true;

	if (m_pBatchJob && m_SpuGroupId != 0)
	{
		__sync_synchronize();

		if (m_pBatchJob->m_Status != SPU_BATCH_STATUS_IDLE &&
			m_pBatchJob->m_Status != SPU_BATCH_STATUS_DONE)
		{

			m_pBatchJob->m_Command = SPU_BATCH_CMD_TERMINATE;
			m_pBatchJob->m_Status = SPU_BATCH_STATUS_BUSY;
			__sync_synchronize();

			if (m_SpuThreadId != 0)
			{
				sysSpuThreadWriteSignal(m_SpuThreadId, 0, 1);
			}

			for (int32 i = 0; i < 1000; i++)
			{
				__sync_synchronize();
				if (m_pBatchJob->m_Status == SPU_BATCH_STATUS_IDLE ||
					m_pBatchJob->m_Status == SPU_BATCH_STATUS_DONE)
				{
					break;
				}

				sysUsleep(1000); // 1ms
			}
		}
	}

	if (m_SpuGroupId != 0)
	{
		sysUsleep(10000); // 10ms

		sysSpuThreadGroupTerminate(m_SpuGroupId, 0);

		uint32 cause = 0;
		uint32 status = 0;
		sysSpuThreadGroupJoin(m_SpuGroupId, &cause, &status);
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		m_SpuGroupId = 0;
	}

	if (m_pBatchJob)
	{
		CUtlMemory::AlignedFree(m_pBatchJob);
		m_pBatchJob = GCMGL_NULL;
	}

	sysSpuImageClose(&m_SpuImage);

	m_SpuThreadId = 0;
}

SPUResult_t CSpuBatchTransformManager::TransformPositions(
	float32* pPositions,
	const CMatrix4* pMatrices,
	uint32 vertexCount,
	uint32 batchCount,
	uint32 floatsPerVertex)
{
	if (batchCount == 0 || vertexCount == 0)
	{
		return SPUResult_t::NotUsed;
	}

	return SubmitJob(
		pPositions,
		pMatrices,
		vertexCount,
		batchCount,
		floatsPerVertex);
}

SPUResult_t CSpuBatchTransformManager::SubmitJob(
	float32* pPositions,
	const CMatrix4* pMatrices,
	uint32 vertexCount,
	uint32 batchCount,
	uint32 floatsPerVertex)
{
	if (!m_pBatchJob || m_IsShuttingDown)
	{
		return SPUResult_t::NotUsed;
	}

	m_pBatchJob->m_VertexCount = vertexCount;
	m_pBatchJob->m_BatchCount = batchCount;
	m_pBatchJob->m_FloatsPerVertex = floatsPerVertex;
	m_pBatchJob->m_MatrixStride = sizeof(CMatrix4);
	m_pBatchJob->m_PositionsEffAddr = SpuUtils::PtrToEa(pPositions);
	m_pBatchJob->m_MatricesEffAddr = SpuUtils::PtrToEa(
		const_cast<CMatrix4*>(pMatrices));
	m_pBatchJob->m_Status = SPU_BATCH_STATUS_BUSY;
	m_pBatchJob->m_Command = SPU_BATCH_CMD_TRANSFORM;

	__sync_synchronize();

	const int32 signalResult = sysSpuThreadWriteSignal(m_SpuThreadId, 0, 1);
	if (signalResult < 0)
	{
		m_pBatchJob->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJob->m_Status = SPU_BATCH_STATUS_ERROR;

		__sync_synchronize();

		return SPUResult_t::Error;
	}

	while (true)
	{
		__sync_synchronize();

		if (m_pBatchJob->m_Status == SPU_BATCH_STATUS_DONE)
		{
			m_pBatchJob->m_Command = SPU_BATCH_CMD_IDLE;
			m_pBatchJob->m_Status = SPU_BATCH_STATUS_IDLE;

			return SPUResult_t::Success;
		}

		if (m_pBatchJob->m_Status == SPU_BATCH_STATUS_ERROR)
		{
			m_pBatchJob->m_Command = SPU_BATCH_CMD_IDLE;

			return SPUResult_t::Error;
		}

		// TODO
		sysUsleep(50);
	}
}