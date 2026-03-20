//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef DBG_H
#define DBG_H

#pragma once

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>

#define Msg(...) printf(__VA_ARGS__)

#define Warning(...) \
	do { printf("[WARNING] "); printf(__VA_ARGS__); } while (0)

#define Error(...) \
	do { printf("[ERROR] "); printf(__VA_ARGS__); } while (0)

// Assert
#if !defined(NDEBUG)

	#define Assert(x) \
		do { \
			if (!(x)) { \
				printf("[ASSERT] %s, line %d: %s\n", \
					__FILE__, __LINE__, #x); \
				fflush(stdout); \
			} \
		} while (0)

	#define AssertMsg(x, msg) \
		do { \
			if (!(x)) { \
				printf("[ASSERT] %s, line %d: %s - %s\n", \
					__FILE__, __LINE__, #x, msg); \
				fflush(stdout); \
			} \
		} while (0)

	#define AssertFatal(x) \
		do { \
			if (!(x)) { \
				printf("[FATAL ASSERT] %s, line %d: %s\n", \
					__FILE__, __LINE__, #x); \
				fflush(stdout); \
				abort(); \
			} \
		} while (0)

#else
	#define Assert(x) do { } while (0)
	#define AssertMsg(x, msg) do { } while (0)
	#define AssertFatal(x) do { if (!(x)) abort(); } while (0)
#endif

#if !defined(NDEBUG)
	#define DebugMsg(...) \
		do { printf("[DEBUG] "); printf(__VA_ARGS__); } while (0)
#else
	#define DebugMsg(...) do { } while (0)
#endif

// Pointer validation
#define IsValidPtr(p) ((p) != GCMGL_NULL)
#define ValidatePtr(p) AssertMsg(IsValidPtr(p), "Invalid pointer: " #p)

#endif // DBG_H