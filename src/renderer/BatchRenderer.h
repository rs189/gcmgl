//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef BATCH_RENDERER_H
#define BATCH_RENDERER_H

#pragma once

#include "Renderer.h"

class IBatchRenderer : public virtual IRenderer
{
public:
	virtual ~IBatchRenderer()
	{
	}

	virtual void DrawBatched(
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startVertex = 0) = 0;
	virtual void DrawIndexedBatched(
		uint32 indexCount,
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startIndex = 0,
		int32 baseVertex = 0) = 0;
};

class CBatchRenderer : public virtual CRenderer, public IBatchRenderer
{
public:
	virtual void DrawBatched(
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startVertex = 0) GCMGL_OVERRIDE;
	virtual void DrawIndexedBatched(
		uint32 indexCount,
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startIndex = 0,
		int32 baseVertex = 0) GCMGL_OVERRIDE;
protected:
	virtual void DrawBatchedChunk(
		uint32 vertexCount,
		const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startVertex) = 0;
	virtual void DrawIndexedBatchedChunk(
		uint32 indexCount,
		uint32 vertexCount,
		const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startIndex,
		int32 baseVertex) = 0;

	void FrustumCullBatch(
		const CBatch& batch,
		const Plane_t* pFrustumPlanes,
		CUtlVector<BatchChunkTransform_t>& batchChunkTransforms);
public:
	static bool ShouldUpdateChunk(float32 distanceToCamera, uint64 frameCount);

	static void TransformVertices(
		char* pDst,
		uint32 vertexCount,
		uint32 vertexStride,
		uint32 vertexPosOffset,
		const CMatrix4& matrix);
	static void ProcessBatch(
		char* pVertexDst,
		const char* pVertexSrc,
		uint32 vertexCount,
		uint32 vertexStride,
		uint32 vertexPosOffset,
		bool hasVertexPos,
		const CMatrix4& matrix);
	static void ProcessIndexedBatch(
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
		const CMatrix4& matrix);
	static bool FindVertexPosOffset(
		const CVertexLayout* pLayout,
		uint32& outOffset);
protected:
	CUtlVector<BatchChunkTransform_t> m_BatchChunkTransformsScratch;
	uint32 m_VertexPosOffset;
	bool m_HasVertexPos;
};

#endif // BATCH_RENDERER_H