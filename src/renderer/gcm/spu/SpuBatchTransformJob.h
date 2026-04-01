//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_BATCH_TRANSFORM_JOB_H
#define SPU_BATCH_TRANSFORM_JOB_H

#pragma once

#include "tier0/platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SPU_BATCH_CMD_IDLE 0u
#define SPU_BATCH_CMD_TRANSFORM 1u
#define SPU_BATCH_CMD_TERMINATE 0xFFFFFFFFu

#define SPU_BATCH_STATUS_IDLE 0u
#define SPU_BATCH_STATUS_BUSY 1u
#define SPU_BATCH_STATUS_DONE 2u
#define SPU_BATCH_STATUS_ERROR 3u

struct SpuBatchTransformJob_t
{
	uint64 m_SrcVerticesEffAddr;
	uint64 m_SrcIndicesEffAddr;
	uint64 m_TransformsEffAddr;
	uint64 m_DstVerticesEffAddr;
	uint64 m_DstIndicesEffAddr;
	uint32 m_Command;
	uint32 m_Status;
	uint32 m_VertexCount;
	uint32 m_IndexCount;
	uint32 m_BatchCount;
	uint32 m_VertexStride;
	uint32 m_TransformStride;
	uint32 m_VertexPositionOffset;
	uint32 m_BaseVertex;
	uint32 m_Reserved0;
};

#ifndef __cplusplus
typedef struct SpuBatchTransformJob_t SpuBatchTransformJob_t;
#endif

#ifdef __cplusplus
}
#endif

#endif // SPU_BATCH_TRANSFORM_JOB_H