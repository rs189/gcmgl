//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
#include "Maths.h"
#include "tier0/dbg.h"

#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
#include <altivec.h>
#undef bool
#endif
#endif

CVector3::CVector3() : m_X(0.0f), m_Y(0.0f), m_Z(0.0f)
{
}

CVector3::CVector3(float32 x, float32 y, float32 z) : m_X(x), m_Y(y), m_Z(z)
{
}

float32& CVector3::operator[](int32 index)
{
	Assert(index >= 0 && index < 3);

	return (&m_X)[index];
}

const float32& CVector3::operator[](int32 index) const
{
	Assert(index >= 0 && index < 3);

	return (&m_X)[index];
}

CVector3& CVector3::operator=(const CVector3& other)
{
	m_X = other.m_X;
	m_Y = other.m_Y;
	m_Z = other.m_Z;
	
	return *this;
}

CVector3& CVector3::operator+=(const CVector3& other)
{
	m_X += other.m_X;
	m_Y += other.m_Y;
	m_Z += other.m_Z;

	return *this;
}

CVector3& CVector3::operator-=(const CVector3& other)
{
	m_X -= other.m_X;
	m_Y -= other.m_Y;
	m_Z -= other.m_Z;

	return *this;
}

CVector3& CVector3::operator*=(const CVector3& other)
{
	m_X *= other.m_X;
	m_Y *= other.m_Y;
	m_Z *= other.m_Z;

	return *this;
}

CVector3& CVector3::operator*=(float32 scalar)
{
	m_X *= scalar;
	m_Y *= scalar;
	m_Z *= scalar;

	return *this;
}

CVector3& CVector3::operator/=(const CVector3& other)
{
	m_X /= other.m_X;
	m_Y /= other.m_Y;
	m_Z /= other.m_Z;

	return *this;
}

CVector3& CVector3::operator/=(float32 scalar)
{
	float32 invScalar = 1.0f / scalar;
	m_X *= invScalar;
	m_Y *= invScalar;
	m_Z *= invScalar;

	return *this;
}

CVector3 CVector3::operator+(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_add(v1, v2);

	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CVector3(m_X + other.m_X, m_Y + other.m_Y, m_Z + other.m_Z);
}

CVector3 CVector3::operator-(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_sub(v1, v2);

	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CVector3(m_X - other.m_X, m_Y - other.m_Y, m_Z - other.m_Z);
}

CVector3 CVector3::operator*(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, v2, zero);

	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CVector3(m_X * other.m_X, m_Y * other.m_Y, m_Z * other.m_Z);
}

CVector3 CVector3::operator*(float32 scalar) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float vs = vec_splats(scalar);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, vs, zero);
	
	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CVector3(m_X * scalar, m_Y * scalar, m_Z * scalar);
}

CVector3 CVector3::operator/(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };
	vector float reciprocal = vec_re(v2);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, reciprocal, zero);

	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CVector3(m_X / other.m_X, m_Y / other.m_Y, m_Z / other.m_Z);
}

CVector3 CVector3::operator/(float32 scalar) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float vs = vec_splats(scalar);
	vector float reciprocal = vec_re(vs);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, reciprocal, zero);

	return CVector3(u.f[0], u.f[1], u.f[2]);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	float32 invScalar = 1.0f / scalar;

	return CVector3(m_X * invScalar, m_Y * invScalar, m_Z * invScalar);
}

bool CVector3::operator==(const CVector3& other) const
{
	return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z;
}

bool CVector3::operator!=(const CVector3& other) const
{
	return !(*this == other);
}

float32 CVector3::LengthXY() const
{
	return CMaths::Sqrt(m_X * m_X + m_Y * m_Y);
}

float32 CVector3::Length() const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v = { m_X, m_Y, m_Z, 0.0f };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v, v, zero);
	float32 sum = u.f[0] + u.f[1] + u.f[2];

	return CMaths::Sqrt(sum);
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return CMaths::Sqrt(m_X * m_X + m_Y * m_Y + m_Z * m_Z);
}

float32 CVector3::LengthSq() const
{
	return m_X * m_X + m_Y * m_Y + m_Z * m_Z;
}

float32 CVector3::Distance(const CVector3& other) const
{
	return CMaths::Sqrt(DistanceSq(other));
}

float32 CVector3::DistanceSq(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };
	vector float diff = vec_sub(v1, v2);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(diff, diff, zero);

	return u.f[0] + u.f[1] + u.f[2];
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	float32 dx = m_X - other.m_X;
	float32 dy = m_Y - other.m_Y;
	float32 dz = m_Z - other.m_Z;

	return dx * dx + dy * dy + dz * dz;
}

float32 CVector3::Dot(const CVector3& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, 0.0f };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, 0.0f };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, v2, zero);

	return u.f[0] + u.f[1] + u.f[2];
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return m_X * other.m_X + m_Y * other.m_Y + m_Z * other.m_Z;
}