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
#else
#include <simde/x86/sse.h>
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
	switch (index)
	{
		case 0: return m_X;
		case 1: return m_Y;
		case 2: return m_Z;
		default:
			Assert(false);

			return m_X;
	}
}

const float32& CVector3::operator[](int32 index) const
{
	switch (index)
	{
		case 0: return m_X;
		case 1: return m_Y;
		case 2: return m_Z;
		default:
			Assert(false);

			return m_X;
	}
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
	result;
	result.v = vec_add(v1, v2);

	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);
	
	simde__m128 result = simde_mm_add_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	return CVector3(m_X + other.m_X, m_Y + other.m_Y, m_Z + other.m_Z);
#endif
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
	result;
	result.v = vec_sub(v1, v2);

	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);
	
	simde__m128 result = simde_mm_sub_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	return CVector3(m_X - other.m_X, m_Y - other.m_Y, m_Z - other.m_Z);
#endif
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
	result;
	result.v = vec_madd(v1, v2, zero);

	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);
	
	simde__m128 result = simde_mm_mul_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	return CVector3(m_X * other.m_X, m_Y * other.m_Y, m_Z * other.m_Z);
#endif
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
	result;
	result.v = vec_madd(v1, vs, zero);
	
	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 vs = simde_mm_set1_ps(scalar);

	simde__m128 result = simde_mm_mul_ps(v1, vs);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	return CVector3(m_X * scalar, m_Y * scalar, m_Z * scalar);
#endif
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
	result;
	result.v = vec_madd(v1, reciprocal, zero);

	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);
	
	simde__m128 result = simde_mm_div_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	return CVector3(m_X / other.m_X, m_Y / other.m_Y, m_Z / other.m_Z);
#endif
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
	result;
	result.v = vec_madd(v1, reciprocal, zero);

	return CVector3(result.f[0], result.f[1], result.f[2]);
#else
	float32 invScalar = 1.0f / scalar;
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 vs = simde_mm_set1_ps(invScalar);

	simde__m128 result = simde_mm_mul_ps(v1, vs);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return CVector3(temp[0], temp[1], temp[2]);
#endif
#else
	float32 invScalar = 1.0f / scalar;

	return CVector3(m_X * invScalar, m_Y * invScalar, m_Z * invScalar);
#endif
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
	result;
	result.v = vec_madd(v, v, zero);
	float32 sum = result.f[0] + result.f[1] + result.f[2];

	return CMaths::Sqrt(sum);
#else
	simde__m128 v = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);

	simde__m128 result = simde_mm_mul_ps(v, v);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);
	float32 sum = temp[0] + temp[1] + temp[2];

	return CMaths::Sqrt(sum);
#endif
#else
	float32 xx = m_X * m_X;
	float32 yy = m_Y * m_Y;
	float32 zz = m_Z * m_Z;

	return CMaths::Sqrt(xx + yy + zz);
#endif
}

float32 CVector3::Distance(const CVector3& other) const
{
	return CMaths::Sqrt(Distance2(other));
}

float32 CVector3::Distance2(const CVector3& other) const
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
	sq;
	sq.v = vec_madd(diff, diff, zero);

	return sq.f[0] + sq.f[1] + sq.f[2];
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);
	simde__m128 diff = simde_mm_sub_ps(v1, v2);
	simde__m128 sq = simde_mm_mul_ps(diff, diff);

	float32 temp[4];
	simde_mm_storeu_ps(temp, sq);

	return temp[0] + temp[1] + temp[2];
#endif
#else
	float32 dx = m_X - other.m_X;
	float32 dy = m_Y - other.m_Y;
	float32 dz = m_Z - other.m_Z;

	return dx * dx + dy * dy + dz * dz;
#endif
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
	result;
	result.v = vec_madd(v1, v2, zero);

	return result.f[0] + result.f[1] + result.f[2];
#else
	simde__m128 v1 = simde_mm_set_ps(0.0f, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(0.0f, other.m_Z, other.m_Y, other.m_X);

	simde__m128 result = simde_mm_mul_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, result);

	return temp[0] + temp[1] + temp[2];
#endif
#else
	return m_X * other.m_X + m_Y * other.m_Y + m_Z * other.m_Z;
#endif
}