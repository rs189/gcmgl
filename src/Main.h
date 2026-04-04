//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef MAIN_H
#define MAIN_H

#pragma once

#include "tier0/platform.h"

//#define GCMGL_DIAGNOSTICS

// Feature toggles
#define GCMGL_SIMD_FRUSTUM_CULL_ENABLED
#define GCMGL_SIMD_TRANSFORM_ENABLED
#define GCMGL_THREADING_FRUSTUM_CULL_ENABLED
#define GCMGL_THREADING_TRANSFORM_ENABLED

#define GCMGL_SPU_BATCH_TRANSFORM_ENABLED

#endif // MAIN_H