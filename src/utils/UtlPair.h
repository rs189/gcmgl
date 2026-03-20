//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTL_PAIR_H
#define UTL_PAIR_H

#pragma once

#include "tier0/platform.h"

template <typename T1, typename T2>
struct UtlPair_t
{
	T1 m_First;
	T2 m_Second;
};

#endif // UTL_PAIR_H