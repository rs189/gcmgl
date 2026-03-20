//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GlBatchRenderer.h"
#include "tier0/dbg.h"
#include "mathsfury/Maths.h"
#include <glad/gl.h>
#include <string.h>

#ifdef SIMD_ENABLED
#include "simde/x86/sse2.h"

static void TransformVerticesSSE(
	char* pDst,
	uint32 vertexCount,
	uint32 stride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	const simde__m128 col0 = simde_mm_loadu_ps(&matrix.m_Data[0]);
	const simde__m128 col1 = simde_mm_loadu_ps(&matrix.m_Data[4]);
	const simde__m128 col2 = simde_mm_loadu_ps(&matrix.m_Data[8]);
	const simde__m128 col3 = simde_mm_loadu_ps(&matrix.m_Data[12]);

	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * stride + vertexPosOffset);

		simde__m128 result = simde_mm_add_ps(
			simde_mm_add_ps(
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[0]), col0),
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[1]), col1)),
			simde_mm_add_ps(
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[2]), col2),
				col3));

		float32 tmp[4];
		simde_mm_storeu_ps(tmp, result);
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
	TransformVerticesSSE(pDst, vertexCount, stride, vertexPosOffset, matrix);
#else
	CBatchRenderer::TransformVertices(
		pDst,
		vertexCount,
		stride,
		vertexPosOffset,
		matrix);
#endif
}

void CGlBatchRenderer::DrawBatchedChunk(
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
	uint32 totalVertexDataSize = totalVertices * stride;

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (uint32(stagingVertexBuffer.m_Data.Count()) < totalVertexDataSize)
	{
		stagingVertexBuffer.m_Data.SetCount(int32(totalVertexDataSize));
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}

		BufferResource_t& stagingVertexBufferResource = m_BufferResources[stagingVertexBuffer.m_hBuffer];
		stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_Data.Base();
		stagingVertexBufferResource.m_hId = stagingVertexBuffer.m_hId;
		stagingVertexBufferResource.m_Size = uint64(
			stagingVertexBuffer.m_Data.Count());
		stagingVertexBufferResource.m_Target = GL_ARRAY_BUFFER;
	}

	memset(
		stagingVertexBuffer.m_Data.Base(),
		0,
		size_t(stagingVertexBuffer.m_Data.Count()));
	char* pDstVertexData = reinterpret_cast<char*>(
		stagingVertexBuffer.m_Data.Base());

	uint32 vertexPosOffset = 0;
	bool hasVertexPos = FindVertexPosOffset(pVertexLayout, vertexPosOffset);

	BatchData_t localBatchData;
	CopyBatchChunk(batchData, chunkStart, chunkSize, localBatchData);

	for (uint32 i = 0; i < chunkSize; i++)
	{
		char* pChunkVertexDst = pDstVertexData + uint64(i) * vertexCount * stride;
		memcpy(pChunkVertexDst, pSrcVertexData, uint64(vertexCount) * stride);
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

	if (stagingVertexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingVertexBuffer.m_hId);
	}

	glBindBuffer(GL_ARRAY_BUFFER, stagingVertexBuffer.m_hId);
	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalVertexDataSize),
		stagingVertexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	BufferResource_t& stagingVertexBufferResource = m_BufferResources[stagingVertexBuffer.m_hBuffer];
	stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_Data.Base();
	stagingVertexBufferResource.m_hId = stagingVertexBuffer.m_hId;
	stagingVertexBufferResource.m_Size = uint64(totalVertexDataSize);
	stagingVertexBufferResource.m_Target = GL_ARRAY_BUFFER;

	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	Draw(totalVertices);

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}

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
}

void CGlBatchRenderer::DrawIndexedBatchedChunk(
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
	uint64 totalVertexDataSize = uint64(totalVertices) * stride;
	uint64 totalIndexDataSize = uint64(totalIndices) * sizeof(uint32);

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (uint64(stagingVertexBuffer.m_Data.Count()) < totalVertexDataSize)
	{
		stagingVertexBuffer.m_Data.SetCount(int32(totalVertexDataSize));
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}

		BufferResource_t& stagingVertexBufferResource = m_BufferResources[stagingVertexBuffer.m_hBuffer];
		stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_Data.Base();
		stagingVertexBufferResource.m_hId = stagingVertexBuffer.m_hId;
		stagingVertexBufferResource.m_Size = uint64(
			stagingVertexBuffer.m_Data.Count());
		stagingVertexBufferResource.m_Target = GL_ARRAY_BUFFER;
	}

	memset(
		stagingVertexBuffer.m_Data.Base(),
		0,
		size_t(stagingVertexBuffer.m_Data.Count()));
	char* pDstVertexData = reinterpret_cast<char*>(
		stagingVertexBuffer.m_Data.Base());

	StagingBuffer_t& stagingIndexBuffer = m_StagingIndexBuffer[m_StagingIndex];
	if (uint64(stagingIndexBuffer.m_Data.Count()) < totalIndexDataSize)
	{
		stagingIndexBuffer.m_Data.SetCount(int32(totalIndexDataSize));
		if (!stagingIndexBuffer.m_hBuffer)
		{
			stagingIndexBuffer.m_hBuffer = m_NextHandle++;
		}

		BufferResource_t& stagingIndexBufferResource = m_BufferResources[stagingIndexBuffer.m_hBuffer];
		stagingIndexBufferResource.m_pPtr = stagingIndexBuffer.m_Data.Base();
		stagingIndexBufferResource.m_hId = stagingIndexBuffer.m_hId;
		stagingIndexBufferResource.m_Size = uint64(
			stagingIndexBuffer.m_Data.Count());
		stagingIndexBufferResource.m_Target = GL_ELEMENT_ARRAY_BUFFER;
	}

	memset(
		stagingIndexBuffer.m_Data.Base(),
		0,
		size_t(stagingIndexBuffer.m_Data.Count()));
	uint32* pDstIndexData = reinterpret_cast<uint32*>(
		stagingIndexBuffer.m_Data.Base());

	uint32 vertexPosOffset = 0;
	bool hasVertexPos = FindVertexPosOffset(pVertexLayout, vertexPosOffset);

	BatchData_t localBatchData;
	CopyBatchChunk(batchData, chunkStart, chunkSize, localBatchData);

	for (uint32 i = 0; i < chunkSize; i++)
	{
		char* pBatchVertexDst = pDstVertexData + uint64(i) * vertexCount * stride;
		uint32* pBatchIndexDst = pDstIndexData + uint64(i) * indexCount;
		memcpy(pBatchVertexDst, pSrcVertexData, uint64(vertexCount) * stride);
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

	if (stagingVertexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingVertexBuffer.m_hId);
	}

	glBindBuffer(GL_ARRAY_BUFFER, stagingVertexBuffer.m_hId);
	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalVertexDataSize),
		stagingVertexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	if (stagingIndexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingIndexBuffer.m_hId);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stagingIndexBuffer.m_hId);
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalIndexDataSize),
		stagingIndexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	BufferResource_t& stagingVertexBufferResource = m_BufferResources[stagingVertexBuffer.m_hBuffer];
	stagingVertexBufferResource.m_pPtr = stagingVertexBuffer.m_Data.Base();
	stagingVertexBufferResource.m_hId = stagingVertexBuffer.m_hId;
	stagingVertexBufferResource.m_Size = totalVertexDataSize;
	stagingVertexBufferResource.m_Target = GL_ARRAY_BUFFER;

	BufferResource_t& stagingIndexBufferResource = m_BufferResources[stagingIndexBuffer.m_hBuffer];
	stagingIndexBufferResource.m_pPtr = stagingIndexBuffer.m_Data.Base();
	stagingIndexBufferResource.m_hId = stagingIndexBuffer.m_hId;
	stagingIndexBufferResource.m_Size = totalIndexDataSize;
	stagingIndexBufferResource.m_Target = GL_ELEMENT_ARRAY_BUFFER;

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

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_hIndexBuffer = hOriginalIndexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_PipelineState.m_IndexOffset = originalIndexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}
