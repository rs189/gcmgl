//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef MATHS_H
#define MATHS_H

#pragma once

#include "tier0/platform.h"
#include <math.h>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4.h"

struct EquirectUV_t
{
	float32 m_U;
	float32 m_V;
};

class CMaths
{
public:
	static const float32 PI;
	static const float32 TwoPI;
	static const float32 Deg2Rad;
	static const float32 Rad2Deg;

	static inline float32 Sqrt(float32 f)
	{
		return sqrtf(f);
	}

	static inline float32 Tan(float32 f)
	{
		return tanf(f);
	}

	static inline float32 Cos(float32 f)
	{
		return cosf(f);
	}

	static inline float32 Sin(float32 f)
	{
		return sinf(f);
	}

	static inline float32 ATan2(float32 y, float32 x)
	{
		return atan2f(y, x);
	}

	static inline float32 ASin(float32 f)
	{
		return asinf(f);
	}

	static inline float32 Abs(float32 a)
	{
		return fabsf(a);
	}

	static inline int32 Abs(int32 a)
	{
		return (a < 0) ? -a : a;
	}

	static inline float32 Min(float32 a, float32 b)
	{
		return fminf(a, b);
	}

	static inline int32 Min(int32 a, int32 b)
	{
		return (a < b) ? a : b;
	}

	static inline uint32 Min(uint32 a, uint32 b)
	{
		return (a < b) ? a : b;
	}

	static inline float32 Max(float32 a, float32 b)
	{
		return fmaxf(a, b);
	}

	static inline int32 Max(int32 a, int32 b)
	{
		return (a > b) ? a : b;
	}

	static inline uint32 Max(uint32 a, uint32 b)
	{
		return (a > b) ? a : b;
	}

	static inline float32 Clamp(float32 value, float32 minVal, float32 maxVal)
	{
		return Max(minVal, Min(value, maxVal));
	}

	static inline int32 Clamp(int32 value, int32 minVal, int32 maxVal)
	{
		return Max(minVal, Min(value, maxVal));
	}

	static inline uint32 Clamp(uint32 value, uint32 minVal, uint32 maxVal)
	{
		return Max(minVal, Min(value, maxVal));
	}

	static CVector3 Normalise(const CVector3& v);
	static CVector3 Cross(const CVector3& a, const CVector3& b);
	static float32 Dot(const CVector3& a, const CVector3& b);

	static CMatrix4 Perspective(
		float32 fovyDeg,
		float32 aspect,
		float32 nearClip,
		float32 farClip);
	static CMatrix4 Orthographic(
		float32 left,
		float32 right,
		float32 bottom,
		float32 top,
		float32 nearClip,
		float32 farClip);
	static CMatrix4 LookAt(
		const CVector3& eye,
		const CVector3& target,
		const CVector3& up);
	static CMatrix4 Translate(const CMatrix4& m, const CVector3& v);
	static CMatrix4 Rotate(
		const CMatrix4& m,
		float32 angleDeg,
		const CVector3& axis);
	static CMatrix4 Scale(const CMatrix4& m, const CVector3& v);

	static EquirectUV_t DirectionToEquirect(const CVector3& dir);
};

#endif // MATHS_H