//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTL_MEMORY_H
#define UTL_MEMORY_H

#pragma once

#include "tier0/platform.h"
#include <malloc.h>
#include <stdlib.h>

class CUtlMemory
{
public:
	/**
	 * Allocates memory from the heap.
	 *
	 * @param size The number of bytes to allocate.
	 * @return A pointer to the allocated memory, or GCMGL_NULL if the
	 *         allocation failed.
	*/
	static INLINE void* Alloc(uint64 size)
	{
		return malloc(static_cast<size_t>(size));
	}

	/**
	 * Frees memory allocated by Alloc.
	 *
	 * @param pPtr A pointer to the memory to free.
	*/
	static INLINE void Free(void* pPtr)
	{
		free(pPtr);
	}

	/**
	 * Allocates aligned memory.
	 *
	 * @param size The number of bytes to allocate.
	 * @param alignment The alignment (must be a power of two).
	 * @return A pointer to the allocated memory, or GCMGL_NULL if the
	 *         allocation failed.
	*/
	static INLINE void* AlignedAlloc(uint64 size, uint32 alignment)
	{
#if defined(PLATFORM_PS3) || defined(PLATFORM_LINUX)
		return memalign(alignment, uint32(size));
#elif defined(_WIN32)
		return _aligned_malloc(
			static_cast<size_t>(size),
			static_cast<size_t>(alignment));
#else
		return memalign(alignment, uint32(size));
#endif
	}

	/**
	 * Frees aligned memory allocated by AlignedAlloc.
	 *
	 * @param pPtr A pointer to the memory to free. Should be GCMGL_NULL
	 *             or a pointer returned by AlignedAlloc.
	*/
	static INLINE void AlignedFree(void* pPtr)
	{
		if (!pPtr)
		{
			return;
		}

#if defined(_WIN32)
		_aligned_free(pPtr);
#else
		free(pPtr);
#endif
	}
};

#endif // UTL_MEMORY_H
