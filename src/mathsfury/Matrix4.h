//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef MATRIX4_H
#define MATRIX4_H

#pragma once

#include "tier0/platform.h"

class CVector3;
class CVector4;

class CMatrix4
{
public:
	alignas(16) float32 m_Data[16];
	
	CMatrix4();
	explicit CMatrix4(float32 diagonal);

	operator const float32*() const
	{
		return m_Data;
	}

	CMatrix4 operator*(const CMatrix4& other) const;
	CMatrix4& operator*=(const CMatrix4& other);
	CMatrix4 operator+(const CMatrix4& other) const;
	CMatrix4 operator-(const CMatrix4& other) const;
	CMatrix4& operator+=(const CMatrix4& other);
	CMatrix4& operator-=(const CMatrix4& other);

	bool operator==(const CMatrix4& other) const;
	bool operator!=(const CMatrix4& other) const;

	CVector4 operator*(const CVector4& v) const;

	CMatrix4 ToTransformMatrix(
		const CVector3& position,
		const CVector3& scale) const;
};

#endif // MATRIX4_H