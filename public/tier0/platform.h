//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef PLATFORM_H
#define PLATFORM_H

#pragma once

// Platforms
#if defined( PLATFORM_PS3 )
    #undef PLATFORM_PS3
    #define PLATFORM_PS3 1
#elif defined( PLATFORM_WINDOWS ) || defined( _WIN32 ) || defined( _WIN64 )
    #undef PLATFORM_WINDOWS
    #define PLATFORM_WINDOWS 1
#elif defined( PLATFORM_LINUX ) || defined( __linux__ )
    #undef PLATFORM_LINUX
    #define PLATFORM_LINUX 1
#elif defined( PLATFORM_MACOS ) || defined( __APPLE__ )
    #undef PLATFORM_MACOS
    #define PLATFORM_MACOS 1
#else
    #error "Unknown platform"
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
    #define GCMGL_MODERN_CPP 1
#endif

// NULL definition
#ifdef GCMGL_MODERN_CPP
    #define GCMGL_NULL nullptr
#else
    #define GCMGL_NULL 0
#endif

#ifndef NULL
    #define NULL 0
#endif

// Override definition
#ifdef GCMGL_MODERN_CPP
    #define GCMGL_OVERRIDE override
#else
    #define GCMGL_OVERRIDE
#endif

// Basic types
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;

#if defined( _MSC_VER )
    typedef signed __int64 int64;
    typedef unsigned __int64 uint64;
#else
    typedef signed long long int64;
    typedef unsigned long long uint64;
#endif

typedef float float32;
typedef double float64;

// Pointer-sized integer types
#if defined( _WIN64 ) || defined( __LP64__ )
    typedef int64 intp;
    typedef uint64 uintp;
#else
    typedef int32 intp;
    typedef uint32 uintp;
#endif

// Alignment macros
#if defined( _MSC_VER )
    #define ALIGN4  __declspec(align(4))
    #define ALIGN8  __declspec(align(8))
    #define ALIGN16 __declspec(align(16))
    #define ALIGN32 __declspec(align(32))
#elif defined( __GNUC__ ) || defined( __clang__ )
    #define ALIGN4  __attribute__((aligned(4)))
    #define ALIGN8  __attribute__((aligned(8)))
    #define ALIGN16 __attribute__((aligned(16)))
    #define ALIGN32 __attribute__((aligned(32)))
#else
    #define ALIGN4
    #define ALIGN8
    #define ALIGN16
    #define ALIGN32
#endif

// Compiler keywords
#if defined( _MSC_VER )
    #define INLINE __inline
    #define NOINLINE __declspec(noinline)
    #define NORETURN __declspec(noreturn)
    #define RESTRICT __restrict
#elif defined( __GNUC__ ) || defined( __clang__ )
    #define INLINE inline
    #define NOINLINE __attribute__((noinline))
    #define NORETURN __attribute__((noreturn))
    #define RESTRICT __restrict__
#else
    #define INLINE inline
    #define NOINLINE
    #define NORETURN
    #define RESTRICT
#endif

// Likely/Unlikely branch hints
#if defined( __GNUC__ ) || defined( __clang__ )
    #define LIKELY( x ) __builtin_expect( !!(x), 1 )
    #define UNLIKELY( x ) __builtin_expect( !!(x), 0 )
#else
    #define LIKELY( x ) (x)
    #define UNLIKELY( x ) (x)
#endif

// Static assert
#ifdef GCMGL_MODERN_CPP
    #define STATIC_ASSERT(x) static_assert(x, #x)
#else
    #define STATIC_ASSERT(x) typedef char static_assertion[(x) ? 1 : -1]
#endif

// Float literal suffix enforcement
#define GCMGL_FLT(x) (x##f)

// Feature toggles
#define SIMD_ENABLED
#define THREADING_ENABLED

#define PS3_SPU_ENABLED

#endif // PLATFORM_H