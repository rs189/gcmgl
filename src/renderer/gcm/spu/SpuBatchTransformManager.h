//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_BATCH_TRANSFORM_MANAGER_H
#define SPU_BATCH_TRANSFORM_MANAGER_H

#pragma once

#include <sys/spu.h>
#include "platform/ps3/spu/SpuCommon.h"
#include "SpuBatchTransformJob.h"

class CMatrix4;

class CSpuBatchTransformManager
{
public:
	CSpuBatchTransformManager();
	~CSpuBatchTransformManager();

	bool Initialize();
	void Shutdown();

	SPUResult_t ProcessBatch(
		const char* pSrcVertices,
		const uint32* pSrcIndices,
		const CMatrix4* pMatrices,
		char* pDstVertices,
		uint32* pDstIndices,
		uint32 vertexCount,
		uint32 indexCount,
		uint32 batchCount,
		uint32 vertexStride,
		uint32 vertexPosOffset,
		uint32 baseVertex);
private:
	SPUResult_t SubmitJob(
		const char* pSrcVertices,
		const uint32* pSrcIndices,
		const CMatrix4* pMatrices,
		char* pDstVertices,
		uint32* pDstIndices,
		uint32 vertexCount,
		uint32 indexCount,
		uint32 batchCount,
		uint32 vertexStride,
		uint32 vertexPosOffset,
		uint32 baseVertex);

	sysSpuImage m_SpuImage;
	uint32 m_SpuGroupId;
	uint32 m_SpuThreadId;
	SpuBatchJob_t* m_pBatchJob;
	bool m_IsShuttingDown;
};

#endif // SPU_BATCH_TRANSFORM_MANAGER_H