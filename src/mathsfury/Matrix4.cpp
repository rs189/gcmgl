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

CMatrix4::CMatrix4() :
	m_Data { 
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f 
	}
{
}

CMatrix4::CMatrix4(float32 diagonal) :
	m_Data {
		diagonal, 0.0f, 0.0f, 0.0f,
		0.0f, diagonal, 0.0f, 0.0f,
		0.0f, 0.0f, diagonal, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
{
}

CMatrix4 CMatrix4::operator*(const CMatrix4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	vector float col0 = vec_lvlx(0, &m_Data[0]);
	col0 = vec_or(col0, vec_lvrx(16, &m_Data[0]));
	
	vector float col1 = vec_lvlx(0, &m_Data[4]);
	col1 = vec_or(col1, vec_lvrx(16, &m_Data[4]));

	vector float col2 = vec_lvlx(0, &m_Data[8]);
	col2 = vec_or(col2, vec_lvrx(16, &m_Data[8]));

	vector float col3 = vec_lvlx(0, &m_Data[12]);
	col3 = vec_or(col3, vec_lvrx(16, &m_Data[12]));

	union
	{
		vector float v;
		float32 f[4];
	}
	u;

	CMatrix4 result;

	for (int32 c = 0; c < 4; c++)
	{
		vector float res = vec_madd(
			col0,
			vec_splats(other.m_Data[c * 4 + 0]),
			zero);
		res = vec_madd(col1, vec_splats(other.m_Data[c * 4 + 1]), res);
		res = vec_madd(col2, vec_splats(other.m_Data[c * 4 + 2]), res);
		res = vec_madd(col3, vec_splats(other.m_Data[c * 4 + 3]), res);
		
		u.v = res;
		result.m_Data[c * 4 + 0] = u.f[0];
		result.m_Data[c * 4 + 1] = u.f[1];
		result.m_Data[c * 4 + 2] = u.f[2];
		result.m_Data[c * 4 + 3] = u.f[3];
	}

	return result;
#else // PLATFORM_PS3
	simde__m128 col0 = simde_mm_loadu_ps(&m_Data[0]);
	simde__m128 col1 = simde_mm_loadu_ps(&m_Data[4]);
	simde__m128 col2 = simde_mm_loadu_ps(&m_Data[8]);
	simde__m128 col3 = simde_mm_loadu_ps(&m_Data[12]);
	
	CMatrix4 result;
	float32 temp[4];
	for (int32 c = 0; c < 4; c++)
	{
		simde__m128 res = simde_mm_mul_ps(
			col0,
			simde_mm_set1_ps(other.m_Data[c * 4 + 0]));
		res = simde_mm_add_ps(
			res,
			simde_mm_mul_ps(
				col1,
				simde_mm_set1_ps(other.m_Data[c * 4 + 1])));
		res = simde_mm_add_ps(
			res,
			simde_mm_mul_ps(
				col2,
				simde_mm_set1_ps(other.m_Data[c * 4 + 2])));
		res = simde_mm_add_ps(
			res,
			simde_mm_mul_ps(
				col3,
				simde_mm_set1_ps(other.m_Data[c * 4 + 3])));

		simde_mm_storeu_ps(temp, res);
		result.m_Data[c * 4 + 0] = temp[0];
		result.m_Data[c * 4 + 1] = temp[1];
		result.m_Data[c * 4 + 2] = temp[2];
		result.m_Data[c * 4 + 3] = temp[3];
	}

	return result;
#endif // !PLATFORM_PS3
#else
	CMatrix4 result;

	for (int32 i = 0; i < 4; i++)
	{
		for (int32 j = 0; j < 4; j++)
		{
			result.m_Data[i * 4 + j] = 
				m_Data[0 * 4 + j] * other.m_Data[i * 4 + 0] +
				m_Data[1 * 4 + j] * other.m_Data[i * 4 + 1] +
				m_Data[2 * 4 + j] * other.m_Data[i * 4 + 2] +
				m_Data[3 * 4 + j] * other.m_Data[i * 4 + 3];
		}
	}

	return result;
#endif
}

CMatrix4& CMatrix4::operator*=(const CMatrix4& other)
{
	*this = (*this) * other;

	return *this;
}

CMatrix4 CMatrix4::operator+(const CMatrix4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	CMatrix4 result;

	for (int32 i = 0; i < 4; i++)
	{
		vector float v1 = { m_Data[i * 4 + 0], m_Data[i * 4 + 1], m_Data[i * 4 + 2], m_Data[i * 4 + 3] };
		vector float v2 = { other.m_Data[i * 4 + 0], other.m_Data[i * 4 + 1], other.m_Data[i * 4 + 2], other.m_Data[i * 4 + 3] };

		union
		{
			vector float v;
			float32 f[4];
		}
		u;
		u.v = vec_add(v1, v2);

		result.m_Data[i * 4 + 0] = u.f[0];
		result.m_Data[i * 4 + 1] = u.f[1];
		result.m_Data[i * 4 + 2] = u.f[2];
		result.m_Data[i * 4 + 3] = u.f[3];
	}

	return result;
#else // PLATFORM_PS3
	CMatrix4 result;

	for (int32 i = 0; i < 4; i++)
	{
		simde__m128 v1 = simde_mm_loadu_ps(&m_Data[i * 4]);
		simde__m128 v2 = simde_mm_loadu_ps(&other.m_Data[i * 4]);
		simde_mm_storeu_ps(&result.m_Data[i * 4], simde_mm_add_ps(v1, v2));
	}

	return result;
#endif // !PLATFORM_PS3
#else
	CMatrix4 result;

	for (int32 i = 0; i < 16; i++)
	{
		result.m_Data[i] = m_Data[i] + other.m_Data[i];
	}

	return result;
#endif
}

CMatrix4 CMatrix4::operator-(const CMatrix4& other) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	CMatrix4 result;

	for (int32 i = 0; i < 4; i++)
	{
		vector float v1 = { m_Data[i * 4 + 0], m_Data[i * 4 + 1], m_Data[i * 4 + 2], m_Data[i * 4 + 3] };
		vector float v2 = { other.m_Data[i * 4 + 0], other.m_Data[i * 4 + 1], other.m_Data[i * 4 + 2], other.m_Data[i * 4 + 3] };
		
		union
		{ 
			vector float v; 
			float32 f[4]; 
		}
		u;
		u.v = vec_sub(v1, v2);

		result.m_Data[i * 4 + 0] = u.f[0];
		result.m_Data[i * 4 + 1] = u.f[1];
		result.m_Data[i * 4 + 2] = u.f[2];
		result.m_Data[i * 4 + 3] = u.f[3];
	}

	return result;
#else // PLATFORM_PS3
	CMatrix4 result;

	for (int32 i = 0; i < 4; i++)
	{
		simde__m128 v1 = simde_mm_loadu_ps(&m_Data[i * 4]);
		simde__m128 v2 = simde_mm_loadu_ps(&other.m_Data[i * 4]);
		simde_mm_storeu_ps(&result.m_Data[i * 4], simde_mm_sub_ps(v1, v2));
	}
	
	return result;
#endif // !PLATFORM_PS3
#else
	CMatrix4 result;

	for (int32 i = 0; i < 16; i++)
	{
		result.m_Data[i] = m_Data[i] - other.m_Data[i];
	}

	return result;
#endif
}

CMatrix4& CMatrix4::operator+=(const CMatrix4& other)
{
	*this = *this + other;

	return *this;
}

CMatrix4& CMatrix4::operator-=(const CMatrix4& other)
{
	*this = *this - other;

	return *this;
}

bool CMatrix4::operator==(const CMatrix4& other) const
{
	const float32 epsilon = 1e-4f;
	for (int32 i = 0; i < 16; i++)
	{
		if (CMaths::Abs(m_Data[i] - other.m_Data[i]) > epsilon)
		{
			return false;
		}
	}

	return true;
}

bool CMatrix4::operator!=(const CMatrix4& other) const
{
	return !(*this == other);
}

CVector4 CMatrix4::operator*(const CVector4& v) const
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float col0 = vec_lvlx(0, &m_Data[0]);
	col0 = vec_or(col0, vec_lvrx(16, &m_Data[0]));
	
	vector float col1 = vec_lvlx(0, &m_Data[4]);
	col1 = vec_or(col1, vec_lvrx(16, &m_Data[4]));

	vector float col2 = vec_lvlx(0, &m_Data[8]);
	col2 = vec_or(col2, vec_lvrx(16, &m_Data[8]));

	vector float col3 = vec_lvlx(0, &m_Data[12]);
	col3 = vec_or(col3, vec_lvrx(16, &m_Data[12]));
	
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float vx = vec_splats(v.m_X);
	vector float vy = vec_splats(v.m_Y);
	vector float vz = vec_splats(v.m_Z);
	vector float vw = vec_splats(v.m_W);
	
	vector float res = vec_madd(vx, col0, zero);
	res = vec_madd(vy, col1, res);
	res = vec_madd(vz, col2, res);
	res = vec_madd(vw, col3, res);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = res;

	return CVector4(u.f[0], u.f[1], u.f[2], u.f[3]);
#else // PLATFORM_PS3
	simde__m128 col0 = simde_mm_loadu_ps(&m_Data[0]);
	simde__m128 col1 = simde_mm_loadu_ps(&m_Data[4]);
	simde__m128 col2 = simde_mm_loadu_ps(&m_Data[8]);
	simde__m128 col3 = simde_mm_loadu_ps(&m_Data[12]);
	
	simde__m128 vx = simde_mm_set1_ps(v.m_X);
	simde__m128 vy = simde_mm_set1_ps(v.m_Y);
	simde__m128 vz = simde_mm_set1_ps(v.m_Z);
	simde__m128 vw = simde_mm_set1_ps(v.m_W);
	
	simde__m128 res = simde_mm_add_ps(
		simde_mm_add_ps(simde_mm_mul_ps(vx, col0), simde_mm_mul_ps(vy, col1)),
		simde_mm_add_ps(simde_mm_mul_ps(vz, col2), simde_mm_mul_ps(vw, col3)));
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector4(temp[0], temp[1], temp[2], temp[3]);
#endif // !PLATFORM_PS3
#else
	return CVector4(
		m_Data[0] * v.m_X + m_Data[4] * v.m_Y + m_Data[8] * v.m_Z + m_Data[12] * v.m_W,
		m_Data[1] * v.m_X + m_Data[5] * v.m_Y + m_Data[9] * v.m_Z + m_Data[13] * v.m_W,
		m_Data[2] * v.m_X + m_Data[6] * v.m_Y + m_Data[10] * v.m_Z + m_Data[14] * v.m_W,
		m_Data[3] * v.m_X + m_Data[7] * v.m_Y + m_Data[11] * v.m_Z + m_Data[15] * v.m_W);
#endif
}

CMatrix4 CMatrix4::ToTransformMatrix(
	const CVector3& position,
	const CVector3& scale) const
{
	Assert(
		CMaths::Abs(m_Data[12]) < 0.001f && CMaths::Abs(m_Data[13]) < 0.001f && CMaths::Abs(m_Data[14]) < 0.001f);

	CMatrix4 result = *this;

	result.m_Data[0] *= scale.m_X;
	result.m_Data[1] *= scale.m_X;
	result.m_Data[2] *= scale.m_X;
	
	result.m_Data[4] *= scale.m_Y;
	result.m_Data[5] *= scale.m_Y;
	result.m_Data[6] *= scale.m_Y;
	
	result.m_Data[8] *= scale.m_Z;
	result.m_Data[9] *= scale.m_Z;
	result.m_Data[10] *= scale.m_Z;
	
	result.m_Data[12] = position.m_X;
	result.m_Data[13] = position.m_Y;
	result.m_Data[14] = position.m_Z;
	
	return result;
}