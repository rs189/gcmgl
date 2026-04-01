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

#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "tier0/platform.h"

static INLINE void waitForTag(uint32 mask)
{
	mfc_write_tag_mask(mask);
	spu_mfcstat(MFC_TAG_UPDATE_ALL);
}

static INLINE void dmaGet(void* ls, uint64 ea, uint32 size, uint32 tag)
{
	if (size == 0)
	{
		return;
	}

	mfc_get(ls, ea, size, tag, 0, 0);
}

static INLINE void dmaPut(void* ls, uint64 ea, uint32 size, uint32 tag)
{
	if (size == 0)
	{
		return;
	}

	mfc_put(ls, ea, size, tag, 0, 0);
}

#endif // SPU_UTILS_H