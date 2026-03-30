//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GcmBatchRenderer.h"
#include "tier0/dbg.h"
#include "mathsfury/Maths.h"
#include <string.h>
#include <malloc.h>
#include <rsx/rsx.h>

#ifdef PS3_SPU_ENABLED
#include "spu/SpuBatchTransformManager.h"
#endif

CGcmBatchRenderer::CGcmBatchRenderer() :
#ifdef PS3_SPU_ENABLED
	m_PendingVertexBuffer(0),
	m_PendingIndexBuffer(0),
	m_PendingTotalVertices(0),
	m_PendingTotalIndices(0),
	m_PendingStartIndex(0),
	m_PendingBaseVertex(0),
	m_HasPendingBatch(false),
	m_IsPendingBatchIndexed(false)
#endif
{
}

CGcmBatchRenderer::~CGcmBatchRenderer()
{
}

#ifdef PS3_SPU_ENABLED
void CGcmBatchRenderer::FlushPendingBatches()
{
	if (!m_HasPendingBatch)
	{
		return;
	}

	if (m_pSpuBatchTransformManager)
	{
		m_pSpuBatchTransformManager->WaitBatch();
	}

	uint64 originalIndexOffset = m_PipelineState.m_IndexOffset;
	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	BufferHandle hOriginalIndexBuffer = m_PipelineState.m_hIndexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_hVertexBuffer = m_PendingVertexBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	if (m_IsPendingBatchIndexed)
	{
		m_PipelineState.m_hIndexBuffer = m_PendingIndexBuffer;
		m_PipelineState.m_IndexOffset = 0;
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::IndexBuffer;

		DrawIndexed(
			m_PendingTotalIndices,
			m_PendingStartIndex,
			m_PendingBaseVertex);
	}
	else
	{
		Draw(m_PendingTotalVertices);
	}

	m_PipelineState.m_IndexOffset = originalIndexOffset;
	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_hIndexBuffer = hOriginalIndexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer | StateDirtyFlags_t::IndexBuffer;

	m_HasPendingBatch = false;
}
#endif // PS3_SPU_ENABLED

#ifndef PS3_SPU_ENABLED
#include <lv2/thread.h>

#ifdef SIMD_ENABLED
#include <altivec.h>

static void TransformVerticesVMX(
	char* pDst,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	__vector float col0 = vec_ld(0, &matrix.m_Data[0]);
	__vector float col1 = vec_ld(0, &matrix.m_Data[4]);
	__vector float col2 = vec_ld(0, &matrix.m_Data[8]);
	__vector float col3 = vec_ld(0, &matrix.m_Data[12]);
	__vector float vx, vy, vz, result;
	float32 tmp[4];

	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * vertexStride + vertexPosOffset);

		vx = vec_splats(pPos[0]);
		vy = vec_splats(pPos[1]);
		vz = vec_splats(pPos[2]);
		result = vec_madd(vx, col0, col3);
		result = vec_madd(vy, col1, result);
		result = vec_madd(vz, col2, result);

		vec_st(result, 0, tmp);
		pPos[0] = tmp[0];
		pPos[1] = tmp[1];
		pPos[2] = tmp[2];
	}
}
#endif // SIMD_ENABLED

static void TransformVertices(
	char* pDst,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
#ifdef SIMD_ENABLED
	TransformVerticesVMX(
		pDst,
		vertexCount,
		vertexStride,
		vertexPosOffset,
		matrix);
#else
	CBatchRenderer::TransformVertices(
		pDst,
		vertexCount,
		vertexStride,
		vertexPosOffset,
		matrix);
#endif
}

static void BatchTransforms(void* pArg)
{
	BatchThreadData_t* pBatchThreadData = static_cast<BatchThreadData_t*>(pArg);
	for (uint32 i = pBatchThreadData->m_ThreadStart; i < pBatchThreadData->m_ThreadEnd; i++)
	{
		char* pDst = pBatchThreadData->m_pDstVertexData + (uint64(i) * pBatchThreadData->m_VertexCount * pBatchThreadData->m_VertexLayoutStride);
		memcpy(
			pDst,
			pBatchThreadData->m_pSrcVertexData,
			uint64(pBatchThreadData->m_VertexCount) * pBatchThreadData->m_VertexLayoutStride);

		if (pBatchThreadData->m_HasVertexPos)
		{
			CMatrix4 batchChunkTransformMatrix = (*pBatchThreadData->m_pBatchChunkTransforms)[pBatchThreadData->m_ChunkStart + i].ToMatrix();
			TransformVertices(
				pDst,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				batchChunkTransformMatrix);
		}
	}

	sysThreadExit(0);
}
#endif // !PS3_SPU_ENABLED

void CGcmBatchRenderer::DrawBatched(
	uint32 vertexCount,
	const CBatch& batch,
	const CMatrix4& viewProjection,
	uint32 startVertex)
{
	CBatchRenderer::DrawBatched(
		vertexCount,
		batch,
		viewProjection,
		startVertex);

#ifdef PS3_SPU_ENABLED
	FlushPendingBatches();
#endif
}

void CGcmBatchRenderer::DrawIndexedBatched(
	uint32 indexCount,
	uint32 vertexCount,
	const CBatch& batch,
	const CMatrix4& viewProjection,
	uint32 startIndex,
	int32 baseVertex)
{
	CBatchRenderer::DrawIndexedBatched(
		indexCount,
		vertexCount,
		batch,
		viewProjection,
		startIndex,
		baseVertex);

#ifdef PS3_SPU_ENABLED
	FlushPendingBatches();
#endif
}

void CGcmBatchRenderer::DrawBatchedChunk(
	uint32 vertexCount,
	const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startVertex)
{
#ifdef PS3_SPU_ENABLED
	FlushPendingBatches();
#endif

	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(
			m_BufferResources.Find(m_PipelineState.m_hVertexBuffer)).m_pPtr) + (uint64(startVertex) * pVertexLayout->GetStride());
	uint32 vertexStride = pVertexLayout->GetStride();
	uint32 totalVertices = vertexCount * chunkSize;
	uint32 alignedVertexSize = ((totalVertices * vertexStride) + 127) & ~127;
	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex()) return;

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (stagingVertexBuffer.m_Size < alignedVertexSize)
	{
		if (stagingVertexBuffer.m_pPtr)
		{
			rsxFree(stagingVertexBuffer.m_pPtr);
		}

		stagingVertexBuffer.m_pPtr = rsxMemalign(128, alignedVertexSize);
		if (!stagingVertexBuffer.m_pPtr)
		{
			Warning(
				"[GcmBatchRenderer] Failed to allocate staging vertex buffer\n");

			return;
		}

		stagingVertexBuffer.m_Size = alignedVertexSize;
		rsxAddressToOffset(
			stagingVertexBuffer.m_pPtr,
			&stagingVertexBuffer.m_Offset);
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}
	}

	char* pDstVertexData = reinterpret_cast<char*>(stagingVertexBuffer.m_pPtr);

	uint32 vertexPosOffset;
	const bool hasVertexPos = FindVertexPosOffset(
		pVertexLayout,
		vertexPosOffset);

#ifdef PS3_SPU_ENABLED
	if (hasVertexPos && m_pSpuBatchTransformManager)
	{
		if (chunkSize > m_ScratchMatricesCapacity)
		{
			if (m_pScratchMatrices)
			{
				free(m_pScratchMatrices);
			}

			m_pScratchMatrices = (CMatrix4*)memalign(
				16, 
				chunkSize * sizeof(CMatrix4));
			m_ScratchMatricesCapacity = chunkSize;
		}

		for (uint32 i = 0; i < chunkSize; i++)
		{
			m_pScratchMatrices[i] = batchChunkTransforms[chunkStart + i].ToMatrix();
		}

		m_pSpuBatchTransformManager->BeginBatch(
			pSrcVertexData,
			GCMGL_NULL,
			m_pScratchMatrices,
			pDstVertexData,
			GCMGL_NULL,
			vertexCount,
			0,
			chunkSize,
			vertexStride,
			vertexPosOffset,
			0);
	}
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			memcpy(
				pDstVertexData + uint64(i) * vertexCount * vertexStride,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);
		}
	}
#else // PS3_SPU_ENABLED
#ifdef THREADING_ENABLED
	const uint32 numThreads = (chunkSize > 1) ? 2 : 1;
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		sys_ppu_thread_t threadIDs[2];
		const uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint32 threadStart = i * batchesPerThread;
			uint32 threadEnd = threadStart + batchesPerThread;
			if (threadEnd > chunkSize) threadEnd = chunkSize;
			if (threadStart >= chunkSize) break;

			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pDstIndexData = GCMGL_NULL;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pSrcIndices = GCMGL_NULL;
			batchThreadData[i].m_pBatchChunkTransforms = &batchChunkTransforms;
			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_ChunkStart = chunkStart;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_IndexCount = 0;
			batchThreadData[i].m_VertexLayoutStride = vertexStride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;

			int32 threadCreateResult = sysThreadCreate(
				&threadIDs[i],
				BatchTransforms,
				&batchThreadData[i],
				1500,
				16 * 1024,
				THREAD_JOINABLE,
				const_cast<char*>("BatchThread"));
			if (threadCreateResult != 0)
			{
				Warning(
					"[GcmBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadCreateResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint64 threadExitCode = 0;
			sysThreadJoin(threadIDs[i], &threadExitCode);
		}
	}
#endif // THREADING_ENABLED
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pChunkVertexDst = pDstVertexData + uint64(i) * vertexCount * vertexStride;
			memcpy(
				pChunkVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);

			if (hasVertexPos)
			{
				TransformVertices(
					pChunkVertexDst,
					vertexCount,
					vertexStride,
					vertexPosOffset,
					batchChunkTransforms[chunkStart + i].ToMatrix());
			}
		}
	}
#endif // !PS3_SPU_ENABLED

	rsxFlushBuffer(context);

	const BufferResource_t stagingVertexBufferResource = {
		stagingVertexBuffer.m_pPtr,
		stagingVertexBuffer.m_Offset,
		alignedVertexSize
	};
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

#ifdef PS3_SPU_ENABLED
	m_PendingVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PendingTotalVertices = totalVertices;
	m_HasPendingBatch = true;
	m_IsPendingBatchIndexed = false;
#else
	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	Draw(totalVertices);

	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;
#endif // PS3_SPU_ENABLED

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}

#ifndef PS3_SPU_ENABLED
static void BatchIndexedTransforms(void* pArg)
{
	BatchThreadData_t* pBatchThreadData = static_cast<BatchThreadData_t*>(pArg);
	for (uint32 i = pBatchThreadData->m_ThreadStart; i < pBatchThreadData->m_ThreadEnd; i++)
	{
		char* pVertexDst = pBatchThreadData->m_pDstVertexData +
			(uint64(i) * pBatchThreadData->m_VertexCount * pBatchThreadData->m_VertexLayoutStride);
		uint32* pIndexDst = pBatchThreadData->m_pDstIndexData +
			(uint64(i) * pBatchThreadData->m_IndexCount);

		memcpy(
			pVertexDst,
			pBatchThreadData->m_pSrcVertexData,
			uint64(pBatchThreadData->m_VertexCount) * pBatchThreadData->m_VertexLayoutStride);

		if (pBatchThreadData->m_HasVertexPos)
		{
			CMatrix4 batchChunkTransformMatrix = (*pBatchThreadData->m_pBatchChunkTransforms)[pBatchThreadData->m_ChunkStart + i].ToMatrix();
			TransformVertices(
				pVertexDst,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				batchChunkTransformMatrix);
		}

		uint32 vertexBase = i * pBatchThreadData->m_VertexCount;
		for (uint32 j = 0; j < pBatchThreadData->m_IndexCount; j++)
		{
			pIndexDst[j] = pBatchThreadData->m_pSrcIndices[j] + vertexBase;
		}
	}

	sysThreadExit(0);
}
#endif // !PS3_SPU_ENABLED

void CGcmBatchRenderer::DrawIndexedBatchedChunk(
	uint32 indexCount,
	uint32 vertexCount,
	const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startIndex,
	int32 baseVertex)
{
#ifdef PS3_SPU_ENABLED
	FlushPendingBatches();
#endif

	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(
			m_BufferResources.Find(m_PipelineState.m_hVertexBuffer)).m_pPtr);
	const uint32* pSrcIndices = reinterpret_cast<const uint32*>(
		reinterpret_cast<const char*>(
			m_BufferResources.Element(
				m_BufferResources.Find(m_PipelineState.m_hIndexBuffer)).m_pPtr) +
		(uint64(startIndex) * sizeof(uint32)));
	uint32 vertexStride = pVertexLayout->GetStride();
	uint32 totalVertices = vertexCount * chunkSize;
	uint32 totalIndices = indexCount * chunkSize;
	uint32 alignedVertexSize = ((totalVertices * vertexStride) + 127) & ~127;
	uint32 alignedIndexSize = ((totalIndices * uint32(sizeof(uint32))) + 127) & ~127;
	int32 vertexBufferIndex = m_BufferResources.Find(m_PipelineState.m_hVertexBuffer);
	int32 indexBufferIndex = m_BufferResources.Find(m_PipelineState.m_hIndexBuffer);

	if (vertexBufferIndex == m_BufferResources.InvalidIndex() || indexBufferIndex == m_BufferResources.InvalidIndex())
	{
		return;
	}

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (stagingVertexBuffer.m_Size < alignedVertexSize)
	{
		if (stagingVertexBuffer.m_pPtr)
		{
			rsxFree(stagingVertexBuffer.m_pPtr);
		}

		stagingVertexBuffer.m_pPtr = rsxMemalign(128, alignedVertexSize);
		if (!stagingVertexBuffer.m_pPtr)
		{
			Warning(
				"[GcmBatchRenderer] Failed to allocate staging vertex buffer\n");

			return;
		}

		stagingVertexBuffer.m_Size = alignedVertexSize;
		rsxAddressToOffset(
			stagingVertexBuffer.m_pPtr,
			&stagingVertexBuffer.m_Offset);
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}
	}

	char* pDstVertexData = reinterpret_cast<char*>(stagingVertexBuffer.m_pPtr);

	StagingBuffer_t& stagingIndexBuffer = m_StagingIndexBuffer[m_StagingIndex];
	if (stagingIndexBuffer.m_Size < alignedIndexSize)
	{
		if (stagingIndexBuffer.m_pPtr)
		{
			rsxFree(stagingIndexBuffer.m_pPtr);
		}

		stagingIndexBuffer.m_pPtr = rsxMemalign(128, alignedIndexSize);
		if (!stagingIndexBuffer.m_pPtr)
		{
			Warning(
				"[GcmBatchRenderer] Failed to allocate staging index buffer\n");

			return;
		}

		stagingIndexBuffer.m_Size = alignedIndexSize;
		rsxAddressToOffset(
			stagingIndexBuffer.m_pPtr,
			&stagingIndexBuffer.m_Offset);
		if (!stagingIndexBuffer.m_hBuffer)
		{
			stagingIndexBuffer.m_hBuffer = m_NextHandle++;
		}
	}

	uint32* pDstIndexData = reinterpret_cast<uint32*>(
		stagingIndexBuffer.m_pPtr);

	uint32 vertexPosOffset;
	const bool hasVertexPos = FindVertexPosOffset(
		pVertexLayout,
		vertexPosOffset);

#ifdef PS3_SPU_ENABLED
	if (hasVertexPos && m_pSpuBatchTransformManager)
	{
		if (chunkSize > m_ScratchMatricesCapacity)
		{
			if (m_pScratchMatrices)
			{
				free(m_pScratchMatrices);
			}

			m_pScratchMatrices = (CMatrix4*)memalign(
				16,
				chunkSize * sizeof(CMatrix4));
			m_ScratchMatricesCapacity = chunkSize;
		}

		for (uint32 i = 0; i < chunkSize; i++)
		{
			m_pScratchMatrices[i] = batchChunkTransforms[chunkStart + i].ToMatrix();
		}

		m_pSpuBatchTransformManager->BeginBatch(
			pSrcVertexData,
			pSrcIndices,
			m_pScratchMatrices,
			pDstVertexData,
			pDstIndexData,
			vertexCount,
			indexCount,
			chunkSize,
			vertexStride,
			vertexPosOffset,
			baseVertex);
	}
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			memcpy(
				pDstVertexData + uint64(i) * vertexCount * vertexStride,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);

			for (uint32 j = 0; j < indexCount; j++)
			{
				pDstIndexData[i * indexCount + j] = pSrcIndices[j] + uint32(baseVertex + int32(i * vertexCount));
			}
		}
	}
#else // PS3_SPU_ENABLED
#ifdef THREADING_ENABLED
	const uint32 numThreads = (chunkSize > 1) ? 2 : 1;
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		sys_ppu_thread_t threadIDs[2];
		const uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint32 threadStart = i * batchesPerThread;
			uint32 threadEnd = threadStart + batchesPerThread;
			if (threadEnd > chunkSize) threadEnd = chunkSize;
			if (threadStart >= chunkSize) break;

			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pDstIndexData = pDstIndexData;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pSrcIndices = pSrcIndices;
			batchThreadData[i].m_pBatchChunkTransforms = &batchChunkTransforms;
			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_ChunkStart = chunkStart;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_IndexCount = indexCount;
			batchThreadData[i].m_VertexLayoutStride = vertexStride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;

			int32 threadCreateResult = sysThreadCreate(
				&threadIDs[i],
				BatchIndexedTransforms,
				&batchThreadData[i],
				1500,
				16 * 1024,
				THREAD_JOINABLE,
				const_cast<char*>("BatchThread"));
			if (threadCreateResult != 0)
			{
				Warning(
					"[GcmBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadCreateResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint64 threadExitCode;
			sysThreadJoin(threadIDs[i], &threadExitCode);
		}
	}
#endif // THREADING_ENABLED
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pBatchVertexDst = pDstVertexData + uint64(i) * vertexCount * vertexStride;
			uint32* pBatchIndexDst = pDstIndexData + uint64(i) * indexCount;
			memcpy(
				pBatchVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);

			if (hasVertexPos)
			{
				TransformVertices(
					pBatchVertexDst,
					vertexCount,
					vertexStride,
					vertexPosOffset,
					batchChunkTransforms[chunkStart + i].ToMatrix());
			}

			uint32 vertexBase = i * vertexCount;
			for (uint32 j = 0; j < indexCount; j++)
			{
				pBatchIndexDst[j] = pSrcIndices[j] + vertexBase;
			}
		}
	}
#endif // !PS3_SPU_ENABLED

	rsxFlushBuffer(context);

	const BufferResource_t stagingVertexBufferResource = {
		stagingVertexBuffer.m_pPtr,
		stagingVertexBuffer.m_Offset,
		alignedVertexSize
	};
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

	const BufferResource_t stagingIndexBufferResource = {
		stagingIndexBuffer.m_pPtr,
		stagingIndexBuffer.m_Offset,
		alignedIndexSize
	};
	m_BufferResources.Insert(
		stagingIndexBuffer.m_hBuffer,
		stagingIndexBufferResource);

#ifdef PS3_SPU_ENABLED
	m_PendingVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PendingIndexBuffer = stagingIndexBuffer.m_hBuffer;
	m_PendingTotalVertices = totalVertices;
	m_PendingTotalIndices = totalIndices;
	m_PendingStartIndex = 0;
	m_PendingBaseVertex = 0;
	m_HasPendingBatch = true;
	m_IsPendingBatchIndexed = true;
#else
	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	BufferHandle hOriginalIndexBuffer = m_PipelineState.m_hIndexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;
	uint64 originalIndexOffset = m_PipelineState.m_IndexOffset;

	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_hIndexBuffer = stagingIndexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	DrawIndexed(totalIndices, 0, 0);

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_hIndexBuffer = hOriginalIndexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_PipelineState.m_IndexOffset = originalIndexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;
#endif // PS3_SPU_ENABLED

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}