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

	BufferHandle m_PendingVertexBuffer;
	BufferHandle m_PendingIndexBuffer;
	uint32 m_PendingTotalVertices;
	uint32 m_PendingTotalIndices;
	uint32 m_PendingStartIndex;
	int32 m_PendingBaseVertex;
	bool m_HasPendingBatch;
	bool m_IsPendingBatchIndexed;
#endif // PS3_SPU_ENABLED
};

#endif // GCM_BATCH_RENDERER_H