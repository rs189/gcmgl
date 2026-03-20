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

struct SpuBatchJob_t
{
	uint32 m_Command;
	uint32 m_Status;
	uint32 m_VertexCount;
	uint32 m_BatchCount;
	uint32 m_FloatsPerVertex;
	uint32 m_MatrixStride;
	uint32 m_Reserved0;
	uint32 m_Reserved1;
	uint64 m_PositionsEffAddr;
	uint64 m_MatricesEffAddr;
};

#ifndef __cplusplus
typedef struct SpuBatchJob_t SpuBatchJob_t;
#endif

#ifdef __cplusplus
}
#endif

#endif // SPU_BATCH_TRANSFORM_JOB_H