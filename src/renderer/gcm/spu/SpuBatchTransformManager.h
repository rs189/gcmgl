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

	SPUResult_t BeginBatch(
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
	SPUResult_t WaitBatch();
private:
	static const uint32 s_NumBatchSpus = 2;
	sysSpuImage m_SpuImage;
	SpuBatchJob_t* m_pBatchJobs[s_NumBatchSpus];
	uint32 m_SpuThreadIds[s_NumBatchSpus];
	uint32 m_SpuGroupId;
	bool m_IsShuttingDown;
};

#endif // SPU_BATCH_TRANSFORM_MANAGER_H