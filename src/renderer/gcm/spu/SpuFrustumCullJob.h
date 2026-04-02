//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_FRUSTUM_CULL_JOB_H
#define SPU_FRUSTUM_CULL_JOB_H

#pragma once

#include "tier0/platform.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define SPU_CULL_CMD_IDLE 0u
#define SPU_CULL_CMD_CULL 1u
#define SPU_CULL_CMD_TERMINATE 0xFFFFFFFFu

#define SPU_CULL_STATUS_IDLE 0u
#define SPU_CULL_STATUS_BUSY 1u
#define SPU_CULL_STATUS_DONE 2u
#define SPU_CULL_STATUS_ERROR 3u

struct SpuFrustumCullJob_t
{
	uint64 m_SrcTransformsEffAddr;
	uint64 m_DstTransformsEffAddr;
	uint64 m_DstCountEffAddr;
	uint32 m_Command;
	uint32 m_Status;
	uint32 m_TransformCount;
	uint32 m_TransformStride;
	float32 m_Planes[6][4]; // (nx, ny, nz, d)
	uint32 m_Reserved0;
	uint32 m_Reserved1;
};

#ifndef __cplusplus
typedef struct SpuFrustumCullJob_t SpuFrustumCullJob_t;
#endif // !__cplusplus

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPU_FRUSTUM_CULL_JOB_H
