//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GCM_BATCH_RENDERER_H
#define GCM_BATCH_RENDERER_H

#pragma once

#include "renderer/gcm/GcmRenderer.h"
#include "renderer/BatchRenderer.h"

class CGcmBatchRenderer : public CGcmRenderer, public CBatchRenderer
{
public:
	CGcmBatchRenderer();
	virtual ~CGcmBatchRenderer();

#ifdef PS3_SPU_ENABLED
	virtual void EndFrame() GCMGL_OVERRIDE;
	virtual void Shutdown() GCMGL_OVERRIDE;
#endif

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
	virtual void FrustumCullBatch(
		const CBatch& batch,
		const Plane_t* pFrustumPlanes,
		CUtlVector<BatchChunkTransform_t>& batchChunkTransforms) GCMGL_OVERRIDE;
	virtual void DrawBatchedChunk(
		uint32 vertexCount,
		const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startVertex) GCMGL_OVERRIDE;
	virtual void DrawIndexedBatchedChunk(
		uint32 indexCount,
		uint32 vertexCount,
		const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startIndex,
		int32 baseVertex) GCMGL_OVERRIDE;
private:
#ifdef PS3_SPU_ENABLED
	void FlushPendingBatches();

	struct PendingDrawState_t
	{
		PipelineState_t m_PipelineState;
		BufferHandle m_hVertexBuffer;
		BufferHandle m_hIndexBuffer;
		uint32 m_TotalVertices;
		uint32 m_TotalIndices;
		uint32 m_StartIndex;
		int32 m_BaseVertex;
		bool m_IsIndexed;
	};

	PendingDrawState_t m_PendingDrawState;
	bool m_HasPendingBatch;
#endif // PS3_SPU_ENABLED
};

#endif // GCM_BATCH_RENDERER_H