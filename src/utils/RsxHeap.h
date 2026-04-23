//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef RSX_HEAP_H
#define RSX_HEAP_H

#pragma once

#include "tier0/platform.h"
#include <rsx/rsx.h>

struct RsxAllocation_t
{
	void* m_pPtr;
	uint32 m_Offset;
	uint32 m_Metadata;

	RsxAllocation_t() :
		m_pPtr(GCMGL_NULL),
		m_Offset(0xffffffff),
		m_Metadata(0xffffffff)
	{
	}
};

class CRsxHeap
{
public:
	CRsxHeap() :
		m_pBase(GCMGL_NULL),
		m_pAllocator(GCMGL_NULL),
		m_BaseRsxOffset(0)
	{
	}

	bool Init(uint32 size);
	void Shutdown();

	RsxAllocation_t Alloc(uint32 size, uint32 alignment);
	void Free(RsxAllocation_t rsxAllocation);
private:
	void* m_pBase;
	void* m_pAllocator;
	uint32 m_BaseRsxOffset;
};

#endif // RSX_HEAP_H