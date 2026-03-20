//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTILS_H
#define UTILS_H

#pragma once

#include "tier0/platform.h"
#include "utils/FixedString.h"
#include "utils/UtlVector.h"

class CUtils
{
public:
	static CFixedString	ReadFile(const CFixedString& path);
	static CUtlVector<uint8> ReadBinaryFile(const CFixedString& path);
};

#endif // UTILS_H