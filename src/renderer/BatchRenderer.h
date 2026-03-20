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
		const BatchData_t& batchData,
		const CMatrix4& viewProj,
		uint32 startVertex = 0) = 0;
	virtual void DrawIndexedBatched(
		uint32 indexCount,
		uint32 vertexCount,
		const BatchData_t& batchData,
		const CMatrix4& viewProj,
		uint32 startIndex = 0,
		int32 baseVertex = 0) = 0;
};

class CBatchRenderer : public virtual CRenderer, public IBatchRenderer
{
public:
	virtual void DrawBatched(
		uint32 vertexCount,
		const BatchData_t& batchData,
		const CMatrix4& viewProj,
		uint32 startVertex = 0) GCMGL_OVERRIDE;
	virtual void DrawIndexedBatched(
		uint32 indexCount,
		uint32 vertexCount,
		const BatchData_t& batchData,
		const CMatrix4& viewProj,
		uint32 startIndex = 0,
		int32 baseVertex = 0) GCMGL_OVERRIDE;
protected:
	virtual void DrawBatchedChunk(
		uint32 vertexCount,
		const BatchData_t& batchData,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startVertex) = 0;
	virtual void DrawIndexedBatchedChunk(
		uint32 indexCount,
		uint32 vertexCount,
		const BatchData_t& batchData,
		uint32 chunkStart,
		uint32 chunkSize,
		uint32 startIndex,
		int32 baseVertex) = 0;

	static void TransformVertices(
		char* pDst,
		uint32 vertexCount,
		uint32 stride,
		uint32 vertexPosOffset,
		const CMatrix4& matrix);
	static void ProcessBatch(
		char* pVertexDst,
		const char* pVertexSrc,
		uint32 vertexCount,
		uint32 stride,
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
		uint32 stride,
		uint32 vertexPosOffset,
		bool hasVertexPos,
		const CMatrix4& matrix);
	static bool FindVertexPosOffset(
		const CVertexLayout* pLayout,
		uint32& outOffset);
	static void CopyBatchChunk(
		const BatchData_t& src,
		uint32 chunkStart,
		uint32 chunkSize,
		BatchData_t& dst);
};

#endif // BATCH_RENDERER_H
