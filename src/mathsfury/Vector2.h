//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef VECTOR2_H
#define VECTOR2_H

#pragma once

#include "tier0/platform.h"

class CVector2
{
public:
	float32 m_X;
	float32 m_Y;

	CVector2();
	CVector2(float32 x, float32 y);

	float32& operator[](int32 index);
	const float32& operator[](int32 index) const;
	
	CVector2& operator=(const CVector2& other);

	CVector2& operator+=(const CVector2& other);
	CVector2& operator-=(const CVector2& other);
	CVector2& operator*=(const CVector2& other);
	CVector2& operator*=(float32 scalar);
	CVector2& operator/=(const CVector2& other);
	CVector2& operator/=(float32 scalar);

	CVector2 operator+(const CVector2& other) const;
	CVector2 operator-(const CVector2& other) const;
	CVector2 operator*(const CVector2& other) const;
	CVector2 operator*(float32 scalar) const;
	CVector2 operator/(const CVector2& other) const;
	CVector2 operator/(float32 scalar) const;

	bool operator==(const CVector2& other) const;
	bool operator!=(const CVector2& other) const;

	float32 Length() const;
	float32 LengthSq() const;
	float32 Distance(const CVector2& other) const;
	float32 DistanceSq(const CVector2& other) const;
};

#endif // VECTOR2_H