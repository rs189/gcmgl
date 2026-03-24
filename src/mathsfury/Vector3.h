//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef VECTOR3_H
#define VECTOR3_H

#pragma once

#include "tier0/platform.h"

class CVector3
{
public:
	float32 m_X;
	float32 m_Y;
	float32 m_Z;

	CVector3();
	CVector3(float32 x, float32 y, float32 z);

	float32& operator[](int32 index);
	const float32& operator[](int32 index) const;
	
	CVector3& operator=(const CVector3& other);

	CVector3& operator+=(const CVector3& other);
	CVector3& operator-=(const CVector3& other);
	CVector3& operator*=(const CVector3& other);
	CVector3& operator*=(float32 scalar);
	CVector3& operator/=(const CVector3& other);
	CVector3& operator/=(float32 scalar);

	CVector3 operator+(const CVector3& other) const;
	CVector3 operator-(const CVector3& other) const;
	CVector3 operator*(const CVector3& other) const;
	CVector3 operator*(float32 scalar) const;
	CVector3 operator/(const CVector3& other) const;
	CVector3 operator/(float32 scalar) const;

	bool operator==(const CVector3& other) const;
	bool operator!=(const CVector3& other) const;

	float32 LengthXY() const;
	float32 Length() const;
	float32 LengthSq() const;
	float32 Distance(const CVector3& other) const;
	float32 DistanceSq(const CVector3& other) const;
	float32 Dot(const CVector3& other) const;
};

#endif // VECTOR3_H