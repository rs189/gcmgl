//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_COMMON_H
#define SPU_COMMON_H

#pragma once

#include "tier0/platform.h"

enum class SPUResult_t : uint8
{
	Success = 0,
	NotUsed = 1,
	Error = 2,
};

#endif // SPU_COMMON_H