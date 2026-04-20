//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GL_BATCH_RENDERER_H
#define GL_BATCH_RENDERER_H

#pragma once

#include "renderer/gl/GlRenderer.h"
#include "renderer/BatchRenderer.h"

class CGlBatchRenderer : public CGlRenderer, public CBatchRenderer
{
protected:
	virtual void FrustumCullBatch(
		const CBatch& batch,
		const Plane_t* pFrustumPlanes,
		CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
		bool isPerInstanceCull = true) GCMGL_OVERRIDE;
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
};

#endif // GL_BATCH_RENDERER_H