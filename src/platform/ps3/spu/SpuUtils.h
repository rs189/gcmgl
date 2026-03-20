//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef SPU_UTILS_H
#define SPU_UTILS_H

#pragma once

#include "tier0/platform.h"

namespace SpuUtils
{
	inline uint64 PtrToEa(void* pPtr)
	{
		return static_cast<uint64>(reinterpret_cast<uintptr_t>(pPtr));
	}
}

#endif // SPU_UTILS_H