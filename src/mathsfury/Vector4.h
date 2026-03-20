//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef VECTOR4_H
#define VECTOR4_H

#pragma once

#include "tier0/platform.h"

class CVector4
{
public:
	float32 m_X;
	float32 m_Y;
	float32 m_Z;
	float32 m_W;

	CVector4();
	CVector4(float32 x, float32 y, float32 z, float32 w);

	float32& operator[](int32 index);
	const float32& operator[](int32 index) const;
	
	CVector4& operator=(const CVector4& other);

	CVector4& operator+=(const CVector4& other);
	CVector4& operator-=(const CVector4& other);
	CVector4& operator*=(const CVector4& other);
	CVector4& operator*=(float32 scalar);
	CVector4& operator/=(const CVector4& other);
	CVector4& operator/=(float32 scalar);

	CVector4 operator+(const CVector4& other) const;
	CVector4 operator-(const CVector4& other) const;
	CVector4 operator*(const CVector4& other) const;
	CVector4 operator*(float32 scalar) const;
	CVector4 operator/(const CVector4& other) const;
	CVector4 operator/(float32 scalar) const;

	bool operator==(const CVector4& other) const;
	bool operator!=(const CVector4& other) const;

	float32 Length() const;
};

#endif // VECTOR4_H