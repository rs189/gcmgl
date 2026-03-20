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

	SPUResult_t TransformPositions(
		float32* pPositions,
		const CMatrix4* pMatrices,
		uint32 vertexCount,
		uint32 batchCount,
		uint32 floatsPerVertex = 4u);
private:
	SPUResult_t SubmitJob(
		float32* pPositions,
		const CMatrix4* pMatrices,
		uint32 vertexCount,
		uint32 batchCount,
		uint32 floatsPerVertex);

	sysSpuImage m_SpuImage;
	uint32 m_SpuGroupId;
	uint32 m_SpuThreadId;
	SpuBatchJob_t* m_pBatchJob;
	bool m_IsShuttingDown;
};

#endif // SPU_BATCH_TRANSFORM_MANAGER_H