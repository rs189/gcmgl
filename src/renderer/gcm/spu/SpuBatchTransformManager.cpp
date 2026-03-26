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
	m_IsShuttingDown(false)
{
	for (uint32 i = 0; i < s_NumBatchSpus; i++)
	{
		m_SpuThreadIds[i] = 0;
		m_pBatchJobs[i] = GCMGL_NULL;
	}
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
		s_NumBatchSpus,
		100,
		&groupAttr);
	if (groupCreateResult < 0)
	{
		sysSpuImageClose(&m_SpuImage);

		return false;
	}

	for (uint32 i = 0; i < s_NumBatchSpus; i++)
	{
		sysSpuThreadAttribute threadAttr = { 0 };
		threadAttr.name = const_cast<char*>("gcmBatchSpu");
		threadAttr.option = SPU_THREAD_ATTR_NONE;

		m_pBatchJobs[i] = static_cast<SpuBatchJob_t*>(
			CUtlMemory::AlignedAlloc(sizeof(SpuBatchJob_t), 128));
		if (!m_pBatchJobs[i])
		{
			sysSpuThreadGroupDestroy(m_SpuGroupId);
			sysSpuImageClose(&m_SpuImage);

			return false;
		}
		memset(m_pBatchJobs[i], 0, sizeof(SpuBatchJob_t));

		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;

		sysSpuThreadArgument threadArg = { 0 };
		threadArg.arg0 = SpuUtils::PtrToEa(m_pBatchJobs[i]);

		const int32 threadInitResult = sysSpuThreadInitialize(
			&m_SpuThreadIds[i],
			m_SpuGroupId,
			i,
			&m_SpuImage,
			&threadAttr,
			&threadArg);
		if (threadInitResult < 0)
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
	}

	const int32 startResult = sysSpuThreadGroupStart(m_SpuGroupId);
	if (startResult < 0)
	{
		for (uint32 i = 0; i < s_NumBatchSpus; i++)
		{
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
		for (uint32 i = 0; i < s_NumBatchSpus; i++)
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
		for (int32 i = 0; i < 1000; i++)
		{
			__sync_synchronize();

			hasActiveJobs = false;

			for (uint32 j = 0; j < s_NumBatchSpus; j++)
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

		uint32 cause = 0;
		uint32 status = 0;
		sysSpuThreadGroupJoin(m_SpuGroupId, &cause, &status);
		sysSpuThreadGroupDestroy(m_SpuGroupId);
		m_SpuGroupId = 0;
	}

	for (uint32 i = 0; i < s_NumBatchSpus; i++)
	{
		if (m_pBatchJobs[i])
		{
			CUtlMemory::AlignedFree(m_pBatchJobs[i]);
			m_pBatchJobs[i] = GCMGL_NULL;
		}
		m_SpuThreadIds[i] = 0;
	}

	sysSpuImageClose(&m_SpuImage);
}

SPUResult_t CSpuBatchTransformManager::ProcessBatch(
	const char* pSrcVertices,
	const uint32* pSrcIndices,
	const CMatrix4* pMatrices,
	char* pDstVertices,
	uint32* pDstIndices,
	uint32 vertexCount,
	uint32 indexCount,
	uint32 batchCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	uint32 baseVertex)
{
	if (batchCount == 0 || vertexCount == 0)
	{
		return SPUResult_t::NotUsed;
	}

	return SubmitJob(
		pSrcVertices,
		pSrcIndices,
		pMatrices,
		pDstVertices,
		pDstIndices,
		vertexCount,
		indexCount,
		batchCount,
		vertexStride,
		vertexPosOffset,
		baseVertex);
}

SPUResult_t CSpuBatchTransformManager::SubmitJob(
	const char* pSrcVertices,
	const uint32* pSrcIndices,
	const CMatrix4* pMatrices,
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

	uint32 remainingBatches = batchCount;
	uint32 batchIndex = 0;

	for (uint32 i = 0; i < s_NumBatchSpus; i++)
	{
		if (!m_pBatchJobs[i])
		{
			continue;
		}

		uint32 chunkBatches = remainingBatches / (s_NumBatchSpus - i);
		if (chunkBatches == 0)
		{
			continue;
		}

		uint64 vertexOffset = uint64(batchIndex) * vertexCount * vertexStride;
		uint64 indexOffset = uint64(batchIndex) * indexCount * sizeof(uint32);
		uint64 matrixOffset = uint64(batchIndex) * sizeof(CMatrix4);

		m_pBatchJobs[i]->m_VertexCount = vertexCount;
		m_pBatchJobs[i]->m_IndexCount = indexCount;
		m_pBatchJobs[i]->m_BatchCount = chunkBatches;
		m_pBatchJobs[i]->m_VertexStride = vertexStride;
		m_pBatchJobs[i]->m_MatrixStride = sizeof(CMatrix4);
		m_pBatchJobs[i]->m_VertexPosOffset = vertexPosOffset;
		m_pBatchJobs[i]->m_BaseVertex = baseVertex + (batchIndex * vertexCount);
		
		m_pBatchJobs[i]->m_SrcVerticesEffAddr = SpuUtils::PtrToEa(const_cast<char*>(pSrcVertices));
		m_pBatchJobs[i]->m_SrcIndicesEffAddr = SpuUtils::PtrToEa(const_cast<uint32*>(pSrcIndices));
		
		m_pBatchJobs[i]->m_MatricesEffAddr = SpuUtils::PtrToEa(const_cast<CMatrix4*>(pMatrices)) + matrixOffset;
		m_pBatchJobs[i]->m_DstVerticesEffAddr = SpuUtils::PtrToEa(pDstVertices) + vertexOffset;
		m_pBatchJobs[i]->m_DstIndicesEffAddr = SpuUtils::PtrToEa(pDstIndices) + indexOffset;
		
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_BUSY;
		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_TRANSFORM;

		batchIndex += chunkBatches;
		remainingBatches -= chunkBatches;
	}

	__sync_synchronize();

	for (uint32 i = 0; i < s_NumBatchSpus; i++)
	{
		if (m_pBatchJobs[i] && m_pBatchJobs[i]->m_Command == SPU_BATCH_CMD_TRANSFORM)
		{
			const int32 signalResult = sysSpuThreadWriteSignal(
				m_SpuThreadIds[i],
				0,
				1);
			if (signalResult < 0)
			{
				m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
				m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_ERROR;
			}
		}
	}

	bool hasActiveJobs = true;
	bool hasBatchError = false;

	while (hasActiveJobs)
	{
		__sync_synchronize();

		hasActiveJobs = false;
		for (uint32 i = 0; i < s_NumBatchSpus; i++)
		{
			if (m_pBatchJobs[i] && m_pBatchJobs[i]->m_Command != SPU_BATCH_CMD_IDLE)
			{
				if (m_pBatchJobs[i]->m_Status == SPU_BATCH_STATUS_DONE)
				{
					m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
					m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;
				}
				else if (m_pBatchJobs[i]->m_Status == SPU_BATCH_STATUS_ERROR)
				{
					m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
					hasBatchError = true;
				}
				else
				{
					hasActiveJobs = true;
				}
			}
		}

		if (hasActiveJobs)
		{
			sysUsleep(50);
		}
	}

	return hasBatchError ? SPUResult_t::Error : SPUResult_t::Success;
}