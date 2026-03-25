//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
#include "Maths.h"
#include "Vector2.h"
#include "tier0/dbg.h"

CVector2::CVector2() : m_X(0.0f), m_Y(0.0f)
{
}

CVector2::CVector2(float32 x, float32 y) : m_X(x), m_Y(y)
{
}

float32& CVector2::operator[](int32 index)
{
	Assert(index >= 0 && index < 2);

	return (&m_X)[index];
}

const float32& CVector2::operator[](int32 index) const
{
	Assert(index >= 0 && index < 2);

	return (&m_X)[index];
}

CVector2& CVector2::operator=(const CVector2& other)
{
	m_X = other.m_X;
	m_Y = other.m_Y;

	return *this;
}

CVector2& CVector2::operator+=(const CVector2& other)
{
	m_X += other.m_X;
	m_Y += other.m_Y;

	return *this;
}

CVector2& CVector2::operator-=(const CVector2& other)
{
	m_X -= other.m_X;
	m_Y -= other.m_Y;

	return *this;
}

CVector2& CVector2::operator*=(const CVector2& other)
{
	m_X *= other.m_X;
	m_Y *= other.m_Y;

	return *this;
}

CVector2& CVector2::operator*=(float32 scalar)
{
	m_X *= scalar;
	m_Y *= scalar;

	return *this;
}

CVector2& CVector2::operator/=(const CVector2& other)
{
	m_X /= other.m_X;
	m_Y /= other.m_Y;

	return *this;
}

CVector2& CVector2::operator/=(float32 scalar)
{
	float32 invScalar = 1.0f / scalar;
	m_X *= invScalar;
	m_Y *= invScalar;

	return *this;
}

CVector2 CVector2::operator+(const CVector2& other) const
{
	return CVector2(m_X + other.m_X, m_Y + other.m_Y);
}

CVector2 CVector2::operator-(const CVector2& other) const
{
	return CVector2(m_X - other.m_X, m_Y - other.m_Y);
}

CVector2 CVector2::operator*(const CVector2& other) const
{
	return CVector2(m_X * other.m_X, m_Y * other.m_Y);
}

CVector2 CVector2::operator*(float32 scalar) const
{
	return CVector2(m_X * scalar, m_Y * scalar);
}

CVector2 CVector2::operator/(const CVector2& other) const
{
	float32 invX = 1.0f / other.m_X;
	float32 invY = 1.0f / other.m_Y;

	return CVector2(m_X * invX, m_Y * invY);
}

CVector2 CVector2::operator/(float32 scalar) const
{
	float32 invScalar = 1.0f / scalar;

	return CVector2(m_X * invScalar, m_Y * invScalar);
}

bool CVector2::operator==(const CVector2& other) const
{
	return m_X == other.m_X && m_Y == other.m_Y;
}

bool CVector2::operator!=(const CVector2& other) const
{
	return !(*this == other);
}

float32 CVector2::Length() const
{
	return CMaths::Sqrt(LengthSq());
}

float32 CVector2::LengthSq() const
{
	return m_X * m_X + m_Y * m_Y;
}

float32 CVector2::Distance(const CVector2& other) const
{
	return CMaths::Sqrt(DistanceSq(other));
}

float32 CVector2::DistanceSq(const CVector2& other) const
{
	float32 dx = m_X - other.m_X;
	float32 dy = m_Y - other.m_Y;

	return dx * dx + dy * dy;
}