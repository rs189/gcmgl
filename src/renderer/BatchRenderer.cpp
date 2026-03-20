//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "BatchRenderer.h"
#include "mathsfury/Maths.h"
#include <cstring>

void CBatchRenderer::DrawBatched(
	uint32 vertexCount,
	const BatchData_t& batchData,
	const CMatrix4& viewProj,
	uint32 startVertex)
{
	if (batchData.GetCount() == 0) return;

	PipelineState_t& pipelineState = m_PipelineState;
	if (pipelineState.m_hVertexBuffer == 0)
	{
		Warning("[Renderer] Invalid vertex buffer bound\n");

		return;
	}

	if (pipelineState.m_pVertexLayout == GCMGL_NULL)
	{
		Warning("[Renderer] Invalid vertex layout bound\n");

		return;
	}

	Plane_t frustumPlanes[6];
	ExtractFrustumPlanes(viewProj, frustumPlanes);

	uint32 totalBatches = batchData.GetCount();
	CUtlVector<uint32> visibleBatches;
	visibleBatches.EnsureCapacity(int32(totalBatches));

	for (uint32 i = 0; i < totalBatches; i++)
	{
		const BatchTransform_t& batchTransform = batchData.m_Transforms[i];
		if (TestAABBFrustum(
			batchTransform.m_Position,
			batchTransform.m_Scale * 0.5f,
			frustumPlanes))
		{
			visibleBatches.AddToTail(i);
		}
	}

	uint32 visibleBatchCount = uint32(visibleBatches.Count());
	if (visibleBatchCount == 0) return;

	BatchData_t visibleBatchData;
	visibleBatchData.m_Transforms.EnsureCapacity(int32(visibleBatchCount));
	for (uint32 i = 0; i < visibleBatchCount; i++)
	{
		visibleBatchData.m_Transforms.AddToTail(
			batchData.m_Transforms[visibleBatches[i]]);
	}

	uint32 stride = pipelineState.m_pVertexLayout->GetStride();
	uint32 maxBatchesPerChunk = CMaths::Max(
		1u,
		uint32((8 * 1024 * 1024) / (vertexCount * stride)));

	for (uint32 i = 0; i < visibleBatchCount; i += maxBatchesPerChunk)
	{
		uint32 chunkSize = CMaths::Min(
			maxBatchesPerChunk,
			visibleBatchCount - i);
		DrawBatchedChunk(
			vertexCount,
			visibleBatchData,
			i,
			chunkSize,
			startVertex);
	}
}

void CBatchRenderer::DrawIndexedBatched(
	uint32 indexCount,
	uint32 vertexCount,
	const BatchData_t& batchData,
	const CMatrix4& viewProj,
	uint32 startIndex,
	int32 baseVertex)
{
	if (batchData.GetCount() == 0) return;

	PipelineState_t& pipelineState = m_PipelineState;
	if (pipelineState.m_hVertexBuffer == 0)
	{
		Warning("[Renderer] Invalid vertex buffer bound\n");

		return;
	}

	if (pipelineState.m_hIndexBuffer == 0)
	{
		Warning("[Renderer] Invalid index buffer bound\n");

		return;
	}

	if (pipelineState.m_pVertexLayout == GCMGL_NULL)
	{
		Warning("[Renderer] Invalid vertex layout bound\n");

		return;
	}

	Plane_t frustumPlanes[6];
	ExtractFrustumPlanes(viewProj, frustumPlanes);

	uint32 totalBatches = batchData.GetCount();
	CUtlVector<uint32> visibleBatches;
	visibleBatches.EnsureCapacity(int32(totalBatches));

	for (uint32 i = 0; i < totalBatches; i++)
	{
		const BatchTransform_t& batchTransform = batchData.m_Transforms[i];
		if (TestAABBFrustum(
			batchTransform.m_Position,
			batchTransform.m_Scale * 0.5f,
			frustumPlanes))
		{
			visibleBatches.AddToTail(i);
		}
	}

	uint32 visibleBatchCount = uint32(visibleBatches.Count());
	if (visibleBatchCount == 0) return;

	BatchData_t visibleBatchData;
	visibleBatchData.m_Transforms.EnsureCapacity(int32(visibleBatchCount));
	for (uint32 i = 0; i < visibleBatchCount; i++)
	{
		visibleBatchData.m_Transforms.AddToTail(
			batchData.m_Transforms[visibleBatches[i]]);
	}

	uint32 stride = pipelineState.m_pVertexLayout->GetStride();
	uint32 batchChunkBytes = (vertexCount * stride) + (indexCount * sizeof(uint32));
	uint32 maxBatchesPerChunk = CMaths::Max(
		1u,
		uint32((8 * 1024 * 1024) / batchChunkBytes));

	for (uint32 i = 0; i < visibleBatchCount; i += maxBatchesPerChunk)
	{
		uint32 chunkSize = CMaths::Min(
			maxBatchesPerChunk,
			visibleBatchCount - i);
		DrawIndexedBatchedChunk(
			indexCount,
			vertexCount,
			visibleBatchData,
			i,
			chunkSize,
			startIndex,
			baseVertex);
	}
}

void CBatchRenderer::TransformVertices(
	char* pDst,
	uint32 vertexCount,
	uint32 stride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * stride + vertexPosOffset);
		CVector4 result = matrix * CVector4(pPos[0], pPos[1], pPos[2], 1.0f);
		pPos[0] = result.m_X;
		pPos[1] = result.m_Y;
		pPos[2] = result.m_Z;
	}
}

void CBatchRenderer::ProcessBatch(
	char* pVertexDst,
	const char* pVertexSrc,
	uint32 vertexCount,
	uint32 stride,
	uint32 vertexPosOffset,
	bool hasVertexPos,
	const CMatrix4& matrix)
{
	memcpy(pVertexDst, pVertexSrc, uint64(vertexCount) * stride);
	if (hasVertexPos)
	{
		TransformVertices(
			pVertexDst,
			vertexCount,
			stride,
			vertexPosOffset,
			matrix);
	}
}

void CBatchRenderer::ProcessIndexedBatch(
	char* pVertexDst,
	const char* pVertexSrc,
	uint32* pIndexDst,
	const uint32* pIndexSrc,
	uint32 vertexCount,
	uint32 indexCount,
	uint32 batchIndex,
	uint32 stride,
	uint32 vertexPosOffset,
	bool hasVertexPos,
	const CMatrix4& matrix)
{
	ProcessBatch(
		pVertexDst,
		pVertexSrc,
		vertexCount,
		stride,
		vertexPosOffset,
		hasVertexPos,
		matrix);

	uint32 vertexBase = batchIndex * vertexCount;
	for (uint32 i = 0; i < indexCount; i++)
	{
		pIndexDst[i] = pIndexSrc[i] + vertexBase;
	}
}

bool CBatchRenderer::FindVertexPosOffset(
	const CVertexLayout* pLayout,
	uint32& outOffset)
{
	const CUtlVector<VertexAttribute_t>& attributes = pLayout->GetAttributes();
	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_Semantic == VertexSemantic_t::Position &&
			attribute.m_Format == VertexFormat_t::Float3)
		{
			outOffset = attribute.m_Offset;

			return true;
		}
	}

	return false;
}

void CBatchRenderer::CopyBatchChunk(
	const BatchData_t& src,
	uint32 chunkStart,
	uint32 chunkSize,
	BatchData_t& dst)
{
	dst.m_Transforms.EnsureCapacity(int32(chunkSize));
	for (uint32 i = chunkStart; i < chunkStart + chunkSize && i < src.GetCount(); i++)
	{
		dst.m_Transforms.AddToTail(src.m_Transforms[i]);
	}
}