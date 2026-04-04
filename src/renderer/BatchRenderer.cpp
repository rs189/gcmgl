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

bool CBatchRenderer::ShouldUpdateChunk(
	float32 distanceToCamera,
	uint64 frameCount)
{
	// Update less frequently the further the chunk is from the camera.
	static const float32 s_IntervalStep = 500.0f;
	const int32 updateInterval = CMaths::Max(
		1,
		int32(distanceToCamera / s_IntervalStep));

	return (frameCount % updateInterval) == 0;
}

void CBatchRenderer::FrustumCullBatch(
	const CBatch& batch,
	const Plane_t* pFrustumPlanes,
	CUtlVector<BatchChunkTransform_t>& batchChunkTransforms)
{
	for (int32 chunkIndex = 0; chunkIndex < batch.m_BatchChunks.Count(); chunkIndex++)
	{
		const BatchChunk_t& batchChunk = batch.m_BatchChunks[chunkIndex];

		if (batch.m_pCameraPos)
		{
			static const float32 s_ChunkCullRadius = 20000.0f;
			static const float32 s_ChunkCullRadiusSq = s_ChunkCullRadius * s_ChunkCullRadius;
			const CVector3 chunkOffset = batchChunk.m_Center - *batch.m_pCameraPos;
			if (chunkOffset.LengthSq() > s_ChunkCullRadiusSq)
			{
				continue;
			}
		}

		const FrustumVisibility_t::Enum frustumVisibility = GetAABBFrustumVisibility(
			batchChunk.m_Center,
			batchChunk.m_Extent,
			pFrustumPlanes);

		if (frustumVisibility == FrustumVisibility_t::Outside)
		{
			continue;
		}

		const int32 batchChunkTransformCount = batchChunk.m_BatchChunkTransforms.Count();

		if (frustumVisibility == FrustumVisibility_t::Inside)
		{
			for (int32 j = 0; j < batchChunkTransformCount; j++)
			{
				batchChunkTransforms.AddToTail(
					batchChunk.m_BatchChunkTransforms[j]);
			}

			continue;
		}

		// Partial intersection — test per instance
		for (int32 j = 0; j < batchChunkTransformCount; j++)
		{
			const BatchChunkTransform_t& batchChunkTransform =
				batchChunk.m_BatchChunkTransforms[j];
			if (TestAABBFrustum(
				batchChunkTransform.m_Position,
				batchChunkTransform.m_Scale * 0.5f,
				pFrustumPlanes))
			{
				batchChunkTransforms.AddToTail(batchChunkTransform);
			}
		}
	}
}

void CBatchRenderer::CullChunk(
	const BatchChunkTransform_t* pSrcTransforms,
	uint32 start,
	uint32 end,
	const Plane_t* pPlanes,
	CUtlVector<BatchChunkTransform_t>& transforms)
{
	for (uint32 instanceIndex = start; instanceIndex < end; instanceIndex++)
	{
		const BatchChunkTransform_t& batchChunkTransform = pSrcTransforms[instanceIndex];
		bool isVisible = true;

		for (int32 i = 0; i < 6; i++)
		{
			const Plane_t& plane = pPlanes[i];
			const float32 radius =
				batchChunkTransform.m_Scale.m_X * 0.5f * CMaths::Abs(plane.m_Normal.m_X) +
				batchChunkTransform.m_Scale.m_Y * 0.5f * CMaths::Abs(plane.m_Normal.m_Y) +
				batchChunkTransform.m_Scale.m_Z * 0.5f * CMaths::Abs(plane.m_Normal.m_Z);
			const float32 dist =
				plane.m_Normal.Dot(batchChunkTransform.m_Position) + plane.m_Distance;

			if (dist + radius < 0.0f)
			{
				isVisible = false;

				break;
			}
		}

		if (isVisible)
		{
			transforms.AddToTail(batchChunkTransform);
		}
	}
}

static const int32 s_DrawChunkSize = 8 * 1024 * 1024; // 8 MB

void CBatchRenderer::DrawBatched(
	uint32 vertexCount,
	const CBatch& batch,
	const CMatrix4& viewProjection,
	uint32 startVertex)
{
	if (batch.GetCount() == 0) return;

	if (m_PipelineState.m_hVertexBuffer == 0)
	{
		Warning("[Renderer] Invalid vertex buffer bound\n");

		return;
	}

	if (m_PipelineState.m_pVertexLayout == GCMGL_NULL)
	{
		Warning("[Renderer] Invalid vertex layout bound\n");

		return;
	}

	Plane_t frustumPlanes[6];
	m_BatchChunkTransformsScratch.RemoveAll();
	m_HasVertexPos = FindVertexPosOffset(
		m_PipelineState.m_pVertexLayout,
		m_VertexPosOffset);
	ExtractFrustumPlanes(viewProjection, frustumPlanes);
	FrustumCullBatch(batch, frustumPlanes, m_BatchChunkTransformsScratch);

	if (m_BatchChunkTransformsScratch.Count() == 0) return;

	const uint32 vertexStride = m_PipelineState.m_pVertexLayout->GetStride();
	const uint32 maxPerSubmit = CMaths::Max(
		1u,
		uint32(s_DrawChunkSize / (vertexCount * vertexStride)));
	const uint32 visibleCount = uint32(m_BatchChunkTransformsScratch.Count());

	for (uint32 i = 0; i < visibleCount; i += maxPerSubmit)
	{
		DrawBatchedChunk(
			vertexCount,
			m_BatchChunkTransformsScratch,
			i,
			CMaths::Min(maxPerSubmit, visibleCount - i),
			startVertex);
	}
}

void CBatchRenderer::DrawIndexedBatched(
	uint32 indexCount,
	uint32 vertexCount,
	const CBatch& batch,
	const CMatrix4& viewProjection,
	uint32 startIndex,
	int32 baseVertex)
{
	if (batch.GetCount() == 0) return;

	if (m_PipelineState.m_hVertexBuffer == 0)
	{
		Warning("[Renderer] Invalid vertex buffer bound\n");

		return;
	}

	if (m_PipelineState.m_hIndexBuffer == 0)
	{
		Warning("[Renderer] Invalid index buffer bound\n");

		return;
	}

	if (m_PipelineState.m_pVertexLayout == GCMGL_NULL)
	{
		Warning("[Renderer] Invalid vertex layout bound\n");

		return;
	}

	Plane_t frustumPlanes[6];
	m_BatchChunkTransformsScratch.RemoveAll();
	m_HasVertexPos = FindVertexPosOffset(
		m_PipelineState.m_pVertexLayout,
		m_VertexPosOffset);
	ExtractFrustumPlanes(viewProjection, frustumPlanes);
	FrustumCullBatch(batch, frustumPlanes, m_BatchChunkTransformsScratch);

	if (m_BatchChunkTransformsScratch.Count() == 0) return;

	const uint32 vertexStride = m_PipelineState.m_pVertexLayout->GetStride();
	const uint32 bytesPerInstance = (vertexCount * vertexStride) + (indexCount * sizeof(uint32));
	const uint32 maxPerSubmit = CMaths::Max(
		1u,
		uint32(s_DrawChunkSize / bytesPerInstance));
	const uint32 visibleCount = uint32(m_BatchChunkTransformsScratch.Count());

	for (uint32 i = 0; i < visibleCount; i += maxPerSubmit)
	{
		DrawIndexedBatchedChunk(
			indexCount,
			vertexCount,
			m_BatchChunkTransformsScratch,
			i,
			CMaths::Min(maxPerSubmit, visibleCount - i),
			startIndex,
			baseVertex);
	}
}

void CBatchRenderer::TransformVertices(
	char* pDst,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * vertexStride + vertexPosOffset);
		const CVector4 result = matrix * CVector4(
			pPos[0],
			pPos[1],
			pPos[2],
			1.0f);
		pPos[0] = result.m_X;
		pPos[1] = result.m_Y;
		pPos[2] = result.m_Z;
	}
}

void CBatchRenderer::ProcessBatch(
	char* pVertexDst,
	const char* pVertexSrc,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	bool hasVertexPos,
	const CMatrix4& matrix)
{
	memcpy(pVertexDst, pVertexSrc, uint64(vertexCount) * vertexStride);
	if (hasVertexPos)
	{
		TransformVertices(
			pVertexDst,
			vertexCount,
			vertexStride,
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
	uint32 vertexStride,
	uint32 vertexPosOffset,
	bool hasVertexPos,
	const CMatrix4& matrix)
{
	ProcessBatch(
		pVertexDst,
		pVertexSrc,
		vertexCount,
		vertexStride,
		vertexPosOffset,
		hasVertexPos,
		matrix);

	const uint32 vertexBase = batchIndex * vertexCount;
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
		if (attribute.m_VertexSemantic == VertexSemantic_t::Position &&
			attribute.m_Format == VertexFormat_t::Float3)
		{
			outOffset = attribute.m_Offset;

			return true;
		}
	}

	return false;
}