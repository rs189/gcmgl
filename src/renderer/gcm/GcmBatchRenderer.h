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
};

#endif // GCM_BATCH_RENDERER_H