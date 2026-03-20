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
#include <rsx/rsx.h>

#ifdef PS3_SPU_ENABLED
#include "spu/SpuBatchTransformManager.h"
#endif

#ifndef PS3_SPU_ENABLED
#include <lv2/thread.h>

#ifdef SIMD_ENABLED
#include <altivec.h>

static void TransformVerticesVMX(
	char* pDst,
	uint32 vertexCount,
	uint32 stride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	__vector float col0 = vec_ld(0, &matrix.m_Data[0]);
	__vector float col1 = vec_ld(0, &matrix.m_Data[4]);
	__vector float col2 = vec_ld(0, &matrix.m_Data[8]);
	__vector float col3 = vec_ld(0, &matrix.m_Data[12]);

	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * stride + vertexPosOffset);

		__vector float vx = vec_splats(pPos[0]);
		__vector float vy = vec_splats(pPos[1]);
		__vector float vz = vec_splats(pPos[2]);
		__vector float result = vec_madd(vx, col0, col3);
		result = vec_madd(vy, col1, result);
		result = vec_madd(vz, col2, result);

		float32 tmp[4];
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
	uint32 stride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
#ifdef SIMD_ENABLED
	TransformVerticesVMX(pDst, vertexCount, stride, vertexPosOffset, matrix);
#else
	CBatchRenderer::TransformVertices(
		pDst,
		vertexCount,
		stride,
		vertexPosOffset,
		matrix);
#endif
}

static void BatchTransforms(void* pArg)
{
	BatchThreadData_t* pBatchThreadData = static_cast<BatchThreadData_t*>(pArg);

	for (uint32 i = pBatchThreadData->m_ThreadStart; i < pBatchThreadData->m_ThreadEnd; i++)
	{
		char* pDst = pBatchThreadData->m_pDstVertexData +
			(uint64(i) * pBatchThreadData->m_VertexCount * pBatchThreadData->m_VertexLayoutStride);
		memcpy(
			pDst,
			pBatchThreadData->m_pSrcVertexData,
			uint64(pBatchThreadData->m_VertexCount) * pBatchThreadData->m_VertexLayoutStride);

		if (pBatchThreadData->m_HasVertexPos)
		{
			CMatrix4 batchTransformMatrix = pBatchThreadData->m_pBD->m_Transforms[i].ToMatrix();
			TransformVertices(
				pDst,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				batchTransformMatrix);
		}
	}

	sysThreadExit(0);
}
#endif // !PS3_SPU_ENABLED

void CGcmBatchRenderer::SpuTransformChunk(
	char* pDstVertexData,
	uint32 chunkSize,
	uint32 vertexCount,
	uint32 stride,
	uint32 vertexPosOffset,
	const BatchData_t& localBatchData)
{
#ifdef PS3_SPU_ENABLED
	uint32 requiredPositions = chunkSize * vertexCount * 4u;
	if (requiredPositions > uint32(m_ScratchPositions.Count()))
	{
		m_ScratchPositions.SetCount(int32(requiredPositions));
	}
	if (m_ScratchPositions.Count() == 0) return;

	float32* pScratchPositions = m_ScratchPositions.Base();

	for (uint32 i = 0; i < chunkSize; i++)
	{
		const char* pBatchVertex = pDstVertexData + uint64(i) * vertexCount * stride;
		float32* pBatchPositions = pScratchPositions + uint64(i) * vertexCount * 4u;
		for (uint32 j = 0; j < vertexCount; j++)
		{
			const float32* pPos = reinterpret_cast<const float32*>(
				pBatchVertex + uint64(j) * stride + vertexPosOffset);
			float32* pOut = pBatchPositions + j * 4u;
			pOut[0] = pPos[0];
			pOut[1] = pPos[1];
			pOut[2] = pPos[2];
			pOut[3] = 1.0f;
		}
	}

	if (chunkSize > uint32(m_ScratchMatrices.Count()))
	{
		m_ScratchMatrices.SetCount(int32(chunkSize));
	}
	if (m_ScratchMatrices.Count() == 0) return;

	CMatrix4* pScratchMatrices = m_ScratchMatrices.Base();
	for (uint32 i = 0; i < chunkSize; i++)
	{
		pScratchMatrices[i] = localBatchData.m_Transforms[i].ToMatrix();
	}

	SPUResult_t spuResult = SPUResult_t::NotUsed;
	CSpuBatchTransformManager* pSpuBtm = m_pSpuBtm;
	if (pSpuBtm)
	{
		spuResult = pSpuBtm->TransformPositions(
			pScratchPositions,
			pScratchMatrices,
			vertexCount,
			chunkSize,
			4u);
	}

	if (spuResult == SPUResult_t::Success)
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pBatchVertex = pDstVertexData + uint64(i) * vertexCount * stride;
			const float32* pBatchPositions = pScratchPositions + uint64(i) * vertexCount * 4u;
			for (uint32 j = 0; j < vertexCount; j++)
			{
				float32* pPos = reinterpret_cast<float32*>(
					pBatchVertex + uint64(j) * stride + vertexPosOffset);
				const float32* pIn = pBatchPositions + j * 4u;
				pPos[0] = pIn[0];
				pPos[1] = pIn[1];
				pPos[2] = pIn[2];
			}
		}
	}
#endif // PS3_SPU_ENABLED
}

void CGcmBatchRenderer::DrawBatchedChunk(
	uint32 vertexCount,
	const BatchData_t& batchData,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startVertex)
{
	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex()) return;

	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	uint32 stride = pVertexLayout->GetStride();
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(vertexBufferIndex).m_pPtr) + (uint64(startVertex) * stride);

	uint32 totalVertices = vertexCount * chunkSize;
	uint32 alignedVertexSize = ((totalVertices * stride) + 127) & ~127;

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

	memset(stagingVertexBuffer.m_pPtr, 0, alignedVertexSize);
	char* pDstVertexData = reinterpret_cast<char*>(stagingVertexBuffer.m_pPtr);

	uint32 vertexPosOffset = 0;
	bool hasVertexPos = FindVertexPosOffset(pVertexLayout, vertexPosOffset);

	BatchData_t localBatchData;
	CopyBatchChunk(batchData, chunkStart, chunkSize, localBatchData);

#ifdef PS3_SPU_ENABLED
	for (uint32 i = 0; i < chunkSize; i++)
	{
		memcpy(
			pDstVertexData + uint64(i) * vertexCount * stride,
			pSrcVertexData,
			uint64(vertexCount) * stride);
	}

	if (hasVertexPos)
	{
		SpuTransformChunk(
			pDstVertexData,
			chunkSize,
			vertexCount,
			stride,
			vertexPosOffset,
			localBatchData);
	}
#else // PS3_SPU_ENABLED
	const uint32 numThreads = (chunkSize > 1) ? 2 : 1;
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		sys_ppu_thread_t threadIDs[2];
		uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint32 threadStart = i * batchesPerThread;
			uint32 threadEnd = threadStart + batchesPerThread;
			if (threadEnd > chunkSize) threadEnd = chunkSize;
			if (threadStart >= chunkSize) break;

			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_VertexLayoutStride = stride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pBD = &localBatchData;

			int32 threadResult = sysThreadCreate(
				&threadIDs[i],
				BatchTransforms,
				&batchThreadData[i],
				1500,
				16 * 1024,
				THREAD_JOINABLE,
				const_cast<char*>("BatchThread"));
			if (threadResult != 0)
			{
				Warning(
					"[GcmBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			u64 threadExitCode;
			sysThreadJoin(threadIDs[i], &threadExitCode);
		}
	}
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pChunkVertexDst = pDstVertexData + uint64(i) * vertexCount * stride;
			memcpy(
				pChunkVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * stride);
			if (hasVertexPos)
			{
				CMatrix4 batchTransformMatrix = localBatchData.m_Transforms[i].ToMatrix();
				TransformVertices(
					pChunkVertexDst,
					vertexCount,
					stride,
					vertexPosOffset,
					batchTransformMatrix);
			}
		}
	}
#endif // !PS3_SPU_ENABLED

	rsxFlushBuffer(context);

	BufferResource_t stagingVertexBufferResource;
	stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_pPtr;
	stagingVertexBufferResource.m_Offset = stagingVertexBuffer.m_Offset;
	stagingVertexBufferResource.m_Size = alignedVertexSize;
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	Draw(totalVertices);

	rsxFlushBuffer(context);
	waitFinish();

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

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
			CMatrix4 batchTransformMatrix = pBatchThreadData->m_pBD->m_Transforms[i].ToMatrix();
			TransformVertices(
				pVertexDst,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				batchTransformMatrix);
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
	const BatchData_t& batchData,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startIndex,
	int32 baseVertex)
{
	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	int32 indexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hIndexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex() ||
		indexBufferIndex == m_BufferResources.InvalidIndex())
	{
		return;
	}

	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	uint32 stride = pVertexLayout->GetStride();
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(vertexBufferIndex).m_pPtr);
	const uint32* pSrcIndices = reinterpret_cast<const uint32*>(
		reinterpret_cast<const char*>(
			m_BufferResources.Element(indexBufferIndex).m_pPtr) +
		(uint64(startIndex) * sizeof(uint32)));

	uint32 totalVertices = vertexCount * chunkSize;
	uint32 totalIndices = indexCount * chunkSize;
	uint32 alignedVertexSize = ((totalVertices * stride) + 127) & ~127;
	uint32 alignedIndexSize = ((totalIndices * uint32(sizeof(uint32))) + 127) & ~127;

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

	memset(stagingVertexBuffer.m_pPtr, 0, alignedVertexSize);
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

	memset(stagingIndexBuffer.m_pPtr, 0, alignedIndexSize);
	uint32* pDstIndexData = reinterpret_cast<uint32*>(
		stagingIndexBuffer.m_pPtr);

	uint32 vertexPosOffset = 0;
	bool hasVertexPos = FindVertexPosOffset(pVertexLayout, vertexPosOffset);

	BatchData_t localBatchData;
	CopyBatchChunk(batchData, chunkStart, chunkSize, localBatchData);

#ifdef PS3_SPU_ENABLED
	for (uint32 i = 0; i < chunkSize; i++)
	{
		memcpy(
			pDstVertexData + uint64(i) * vertexCount * stride,
			pSrcVertexData,
			uint64(vertexCount) * stride);

		for (uint32 j = 0; j < indexCount; j++)
		{
			pDstIndexData[i * indexCount + j] =
				pSrcIndices[j] + uint32(baseVertex + int32(i * vertexCount));
		}
	}

	if (hasVertexPos)
	{
		SpuTransformChunk(
			pDstVertexData,
			chunkSize,
			vertexCount,
			stride,
			vertexPosOffset,
			localBatchData);
	}
#else // PS3_SPU_ENABLED
	const uint32 numThreads = (chunkSize > 1) ? 2 : 1;
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		sys_ppu_thread_t threadIDs[2];
		uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			uint32 threadStart = i * batchesPerThread;
			uint32 threadEnd = threadStart + batchesPerThread;
			if (threadEnd > chunkSize) threadEnd = chunkSize;
			if (threadStart >= chunkSize) break;

			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_IndexCount = indexCount;
			batchThreadData[i].m_VertexLayoutStride = stride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pSrcIndices = pSrcIndices;
			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pDstIndexData = pDstIndexData;
			batchThreadData[i].m_pBD = &localBatchData;

			int32 threadResult = sysThreadCreate(
				&threadIDs[i],
				BatchIndexedTransforms,
				&batchThreadData[i],
				1500,
				16 * 1024,
				THREAD_JOINABLE,
				const_cast<char*>("BatchThread"));
			if (threadResult != 0)
			{
				Warning(
					"[GcmBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			u64 threadExitCode;
			sysThreadJoin(threadIDs[i], &threadExitCode);
		}
	}
	else
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pBatchVertexDst = pDstVertexData + uint64(i) * vertexCount * stride;
			uint32* pBatchIndexDst = pDstIndexData + uint64(i) * indexCount;
			memcpy(
				pBatchVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * stride);
			if (hasVertexPos)
			{
				CMatrix4 batchTransformMatrix = localBatchData.m_Transforms[i].ToMatrix();
				TransformVertices(
					pBatchVertexDst,
					vertexCount,
					stride,
					vertexPosOffset,
					batchTransformMatrix);
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

	BufferResource_t stagingVertexBufferResource;
	stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_pPtr;
	stagingVertexBufferResource.m_Offset = stagingVertexBuffer.m_Offset;
	stagingVertexBufferResource.m_Size = alignedVertexSize;
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

	BufferResource_t stagingIndexBufferResource;
	stagingIndexBufferResource.m_pPtr = stagingIndexBuffer.m_pPtr;
	stagingIndexBufferResource.m_Offset = stagingIndexBuffer.m_Offset;
	stagingIndexBufferResource.m_Size = alignedIndexSize;
	m_BufferResources.Insert(
		stagingIndexBuffer.m_hBuffer,
		stagingIndexBufferResource);

	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	BufferHandle hOriginalIndexBuffer = m_PipelineState.m_hIndexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;
	uint64 originalIndexOffset = m_PipelineState.m_IndexOffset;

	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_hIndexBuffer = stagingIndexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_PipelineState.m_IndexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	DrawIndexed(totalIndices);

	rsxFlushBuffer(context);
	waitFinish();

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_hIndexBuffer = hOriginalIndexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_PipelineState.m_IndexOffset = originalIndexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}
