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

CVector4::CVector4() : m_X(0.0f), m_Y(0.0f), m_Z(0.0f), m_W(0.0f)
{
}

CVector4::CVector4(
	float32 x,
	float32 y,
	float32 z,
	float32 w
) : m_X(x), m_Y(y), m_Z(z), m_W(w)
{
}

float32& CVector4::operator[](int32 index)
{
	switch (index)
	{
		case 0: return m_X;
		case 1: return m_Y;
		case 2: return m_Z;
		case 3: return m_W;
		default:
			Assert(false);

			return m_X;
	}
}

const float32& CVector4::operator[](int32 index) const
{
	switch (index)
	{
		case 0: return m_X;
		case 1: return m_Y;
		case 2: return m_Z;
		case 3: return m_W;
		default:
			Assert(false);

			return m_X;
	}
}

CVector4& CVector4::operator=(const CVector4& other)
{
	m_X = other.m_X;
	m_Y = other.m_Y;
	m_Z = other.m_Z;
	m_W = other.m_W;
	
	return *this;
}

CVector4& CVector4::operator+=(const CVector4& other)
{
	m_X += other.m_X;
	m_Y += other.m_Y;
	m_Z += other.m_Z;
	m_W += other.m_W;

	return *this;
}

CVector4& CVector4::operator-=(const CVector4& other)
{
	m_X -= other.m_X;
	m_Y -= other.m_Y;
	m_Z -= other.m_Z;
	m_W -= other.m_W;

	return *this;
}

CVector4& CVector4::operator*=(const CVector4& other)
{
	m_X *= other.m_X;
	m_Y *= other.m_Y;
	m_Z *= other.m_Z;
	m_W *= other.m_W;

	return *this;
}

CVector4& CVector4::operator*=(float32 scalar)
{
	m_X *= scalar;
	m_Y *= scalar;
	m_Z *= scalar;
	m_W *= scalar;

	return *this;
}

CVector4& CVector4::operator/=(const CVector4& other)
{
	m_X /= other.m_X;
	m_Y /= other.m_Y;
	m_Z /= other.m_Z;
	m_W /= other.m_W;

	return *this;
}

CVector4& CVector4::operator/=(float32 scalar)
{
	float32 invScalar = 1.0f / scalar;
	m_X *= invScalar;
	m_Y *= invScalar;
	m_Z *= invScalar;
	m_W *= invScalar;

	return *this;
}

CVector4 CVector4::operator+(const CVector4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, other.m_W };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_add(v1, v2);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(
		other.m_W,
		other.m_Z,
		other.m_Y,
		other.m_X);
	
	simde__m128 res = simde_mm_add_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(
		m_X + other.m_X,
		m_Y + other.m_Y,
		m_Z + other.m_Z,
		m_W + other.m_W);
#endif
}

CVector4 CVector4::operator-(const CVector4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, other.m_W };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_sub(v1, v2);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(
		other.m_W,
		other.m_Z,
		other.m_Y,
		other.m_X);
	
	simde__m128 res = simde_mm_sub_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(
		m_X - other.m_X,
		m_Y - other.m_Y,
		m_Z - other.m_Z,
		m_W - other.m_W);
#endif
}

CVector4 CVector4::operator*(const CVector4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, other.m_W };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, v2, zero);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(
		other.m_W,
		other.m_Z,
		other.m_Y,
		other.m_X);
	
	simde__m128 res = simde_mm_mul_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(
		m_X * other.m_X,
		m_Y * other.m_Y,
		m_Z * other.m_Z,
		m_W * other.m_W);
#endif
}

CVector4 CVector4::operator*(float32 scalar) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float vs = vec_splats(scalar);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, vs, zero);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 vs = simde_mm_set1_ps(scalar);

	simde__m128 res = simde_mm_mul_ps(v1, vs);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(m_X * scalar, m_Y * scalar, m_Z * scalar, m_W * scalar);
#endif
}

CVector4 CVector4::operator/(const CVector4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, other.m_W };
	vector float reciprocal = vec_re(v2);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, reciprocal, zero);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(
		other.m_W,
		other.m_Z,
		other.m_Y,
		other.m_X);
	
	simde__m128 res = simde_mm_div_ps(v1, v2);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);
	
	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(
		m_X / other.m_X,
		m_Y / other.m_Y,
		m_Z / other.m_Z,
		m_W / other.m_W);
#endif
}

CVector4 CVector4::operator/(float32 scalar) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float vs = vec_splats(scalar);
	vector float reciprocal = vec_re(vs);
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v1, reciprocal, zero);

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	float32 invScalar = 1.0f / scalar;
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 vs = simde_mm_set1_ps(invScalar);

	simde__m128 res = simde_mm_mul_ps(v1, vs);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	float32 invScalar = 1.0f / scalar;

	return CVector4(
		m_X * invScalar,
		m_Y * invScalar,
		m_Z * invScalar,
		m_W * invScalar);
#endif
}

bool CVector4::operator==(const CVector4& other) const
{
	return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z && m_W == other.m_W;
}

bool CVector4::operator!=(const CVector4& other) const
{
	return !(*this == other);
}

float32 CVector4::Length() const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v = { m_X, m_Y, m_Z, m_W };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v, v, zero);

	float32 sum = u.f[0] + u.f[1] + u.f[2] + u.f[3];

	return CMaths::Sqrt(sum);
#else // PLATFORM_PS3
	simde__m128 v = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);

	simde__m128 res = simde_mm_mul_ps(v, v);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	float32 sum = temp[0] + temp[1] + temp[2] + temp[3];

	return CMaths::Sqrt(sum);
#endif // !PLATFORM_PS3
#else
	float32 xx = m_X * m_X;
	float32 yy = m_Y * m_Y;
	float32 zz = m_Z * m_Z;
	float32 ww = m_W * m_W;

	return CMaths::Sqrt(xx + yy + zz + ww);
#endif
}

float32 CVector4::LengthSq() const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v = { m_X, m_Y, m_Z, m_W };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(v, v, zero);

	return u.f[0] + u.f[1] + u.f[2] + u.f[3];
#else // PLATFORM_PS3
	simde__m128 v = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);

	simde__m128 res = simde_mm_mul_ps(v, v);
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return temp[0] + temp[1] + temp[2] + temp[3];
#endif // !PLATFORM_PS3
#else
	return m_X * m_X + m_Y * m_Y + m_Z * m_Z + m_W * m_W;
#endif
}

float32 CVector4::Distance(const CVector4& other) const
{
	return CMaths::Sqrt(DistanceSq(other));
}

float32 CVector4::DistanceSq(const CVector4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float v1 = { m_X, m_Y, m_Z, m_W };
	vector float v2 = { other.m_X, other.m_Y, other.m_Z, other.m_W };
	vector float diff = vec_sub(v1, v2);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(diff, diff, zero);

	return u.f[0] + u.f[1] + u.f[2] + u.f[3];
#else // PLATFORM_PS3
	simde__m128 v1 = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 v2 = simde_mm_set_ps(
		other.m_W,
		other.m_Z,
		other.m_Y,
		other.m_X);
	simde__m128 diff = simde_mm_sub_ps(v1, v2);
	simde__m128 res = simde_mm_mul_ps(diff, diff);

	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return temp[0] + temp[1] + temp[2] + temp[3];
#endif // !PLATFORM_PS3
#else
	float32 dx = m_X - other.m_X;
	float32 dy = m_Y - other.m_Y;
	float32 dz = m_Z - other.m_Z;
	float32 dw = m_W - other.m_W;

	return dx * dx + dy * dy + dz * dz + dw * dw;
#endif
}