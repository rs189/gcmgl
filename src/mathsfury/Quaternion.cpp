//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
#include "Maths.h"
#include "Quaternion.h"

#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
#include <altivec.h>
#undef bool
#else
#include <simde/x86/sse.h>
#endif
#endif

CQuaternion::CQuaternion() : m_X(0.0f), m_Y(0.0f), m_Z(0.0f), m_W(1.0f)
{
}

CQuaternion::CQuaternion(
	float32 x,
	float32 y,
	float32 z,
	float32 w
) : m_X(x), m_Y(y), m_Z(z), m_W(w)
{
}

CQuaternion CQuaternion::operator*(const CQuaternion& other) const
{
	CQuaternion result;
	result.m_W = m_W * other.m_W - m_X * other.m_X - m_Y * other.m_Y - m_Z * other.m_Z;
	result.m_X = m_W * other.m_X + m_X * other.m_W + m_Y * other.m_Z - m_Z * other.m_Y;
	result.m_Y = m_W * other.m_Y - m_X * other.m_Z + m_Y * other.m_W + m_Z * other.m_X;
	result.m_Z = m_W * other.m_Z + m_X * other.m_Y - m_Y * other.m_X + m_Z * other.m_W;

	return result;
}

CMatrix4 CQuaternion::ToMatrix() const
{
#ifdef SIMD_ENABLED
	float32 xx, yy, zz;

#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float q = { m_X, m_Y, m_Z, m_W };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(q, q, zero);

	xx = u.f[0];
	yy = u.f[1];
	zz = u.f[2];

#else // PLATFORM_PS3
	simde__m128 q = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);

	simde__m128 res = simde_mm_mul_ps(q, q);

	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	xx = temp[0];
	yy = temp[1];
	zz = temp[2];

#endif // !PLATFORM_PS3
	float32 xy = m_X * m_Y;
	float32 xz = m_X * m_Z;
	float32 yz = m_Y * m_Z;
	float32 wx = m_W * m_X;
	float32 wy = m_W * m_Y;
	float32 wz = m_W * m_Z;

	CMatrix4 result;

	result.m_Data[0] = 1.0f - 2.0f * (yy + zz);
	result.m_Data[1] = 2.0f * (xy + wz);
	result.m_Data[2] = 2.0f * (xz - wy);
	result.m_Data[3] = 0.0f;

	result.m_Data[4] = 2.0f * (xy - wz);
	result.m_Data[5] = 1.0f - 2.0f * (xx + zz);
	result.m_Data[6] = 2.0f * (yz + wx);
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = 2.0f * (xz + wy);
	result.m_Data[9] = 2.0f * (yz - wx);
	result.m_Data[10] = 1.0f - 2.0f * (xx + yy);
	result.m_Data[11] = 0.0f;

	result.m_Data[12] = 0.0f;
	result.m_Data[13] = 0.0f;
	result.m_Data[14] = 0.0f;
	result.m_Data[15] = 1.0f;

	return result;
#else
	float32 xx = m_X * m_X;
	float32 yy = m_Y * m_Y;
	float32 zz = m_Z * m_Z;
	float32 xy = m_X * m_Y;
	float32 xz = m_X * m_Z;
	float32 yz = m_Y * m_Z;
	float32 wx = m_W * m_X;
	float32 wy = m_W * m_Y;
	float32 wz = m_W * m_Z;

	CMatrix4 result;

	result.m_Data[0] = 1.0f - 2.0f * (yy + zz);
	result.m_Data[1] = 2.0f * (xy + wz);
	result.m_Data[2] = 2.0f * (xz - wy);
	result.m_Data[3] = 0.0f;

	result.m_Data[4] = 2.0f * (xy - wz);
	result.m_Data[5] = 1.0f - 2.0f * (xx + zz);
	result.m_Data[6] = 2.0f * (yz + wx);
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = 2.0f * (xz + wy);
	result.m_Data[9] = 2.0f * (yz - wx);
	result.m_Data[10] = 1.0f - 2.0f * (xx + yy);
	result.m_Data[11] = 0.0f;

	result.m_Data[12] = 0.0f;
	result.m_Data[13] = 0.0f;
	result.m_Data[14] = 0.0f;
	result.m_Data[15] = 1.0f;

	return result;
#endif
}

CMatrix4 CQuaternion::ToTransformMatrix(
	const CVector3& position,
	const CVector3& scale
) const
{
	float32 xx = m_X * m_X;
	float32 yy = m_Y * m_Y;
	float32 zz = m_Z * m_Z;
	float32 xy = m_X * m_Y;
	float32 xz = m_X * m_Z;
	float32 yz = m_Y * m_Z;
	float32 wx = m_W * m_X;
	float32 wy = m_W * m_Y;
	float32 wz = m_W * m_Z;

	CMatrix4 result;

	result.m_Data[0] = (1.0f - 2.0f * (yy + zz)) * scale.m_X;
	result.m_Data[1] = (2.0f * (xy + wz)) * scale.m_X;
	result.m_Data[2] = (2.0f * (xz - wy)) * scale.m_X;
	result.m_Data[3] = 0.0f;

	result.m_Data[4] = (2.0f * (xy - wz)) * scale.m_Y;
	result.m_Data[5] = (1.0f - 2.0f * (xx + zz)) * scale.m_Y;
	result.m_Data[6] = (2.0f * (yz + wx)) * scale.m_Y;
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = (2.0f * (xz + wy)) * scale.m_Z;
	result.m_Data[9] = (2.0f * (yz - wx)) * scale.m_Z;
	result.m_Data[10] = (1.0f - 2.0f * (xx + yy)) * scale.m_Z;
	result.m_Data[11] = 0.0f;

	result.m_Data[12] = position.m_X;
	result.m_Data[13] = position.m_Y;
	result.m_Data[14] = position.m_Z;
	result.m_Data[15] = 1.0f;

	return result;
}

CQuaternion& CQuaternion::Normalise()
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float q = { m_X, m_Y, m_Z, m_W };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(q, q, zero);

	float32 len = CMaths::Sqrt(u.f[0] + u.f[1] + u.f[2] + u.f[3]);
	if (len > 1e-8f)
	{
		vector float invLenVec = vec_splats(1.0f / len);

		u.v = vec_madd(q, invLenVec, zero);

		m_X = u.f[0];
		m_Y = u.f[1];
		m_Z = u.f[2];
		m_W = u.f[3];
	}

	return *this;
#else // PLATFORM_PS3
	simde__m128 q = simde_mm_set_ps(m_W, m_Z, m_Y, m_X);
	simde__m128 res = simde_mm_mul_ps(q, q);
	
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	float32 len = CMaths::Sqrt(temp[0] + temp[1] + temp[2] + temp[3]);
	if (len > 1e-8f)
	{
		float32 invLen = 1.0f / len;
		simde__m128 invLenVec = simde_mm_set1_ps(invLen);
		res = simde_mm_mul_ps(q, invLenVec);
		simde_mm_storeu_ps(temp, res);

		m_X = temp[0];
		m_Y = temp[1];
		m_Z = temp[2];
		m_W = temp[3];
	}

	return *this;
#endif // !PLATFORM_PS3
#else
	float32 len = Length();
	if (len > 1e-8f)
	{
		float32 invLen = 1.0f / len;
		m_X *= invLen;
		m_Y *= invLen;
		m_Z *= invLen;
		m_W *= invLen;
	}

	return *this;
#endif
}

float32 CQuaternion::Length() const
{
	return CMaths::Sqrt(m_X * m_X + m_Y * m_Y + m_Z * m_Z + m_W * m_W);
}

CQuaternion CQuaternion::FromEuler(float32 pitch, float32 yaw, float32 roll)
{
	float32 cy = CMaths::Cos(yaw * 0.5f);
	float32 sy = CMaths::Sin(yaw * 0.5f);
	float32 cp = CMaths::Cos(pitch * 0.5f);
	float32 sp = CMaths::Sin(pitch * 0.5f);
	float32 cr = CMaths::Cos(roll * 0.5f);
	float32 sr = CMaths::Sin(roll * 0.5f);

	CQuaternion result;

	result.m_W = cr * cp * cy + sr * sp * sy;
	result.m_X = sr * cp * cy - cr * sp * sy;
	result.m_Y = cr * sp * cy + sr * cp * sy;
	result.m_Z = cr * cp * sy - sr * sp * cy;

	return result;
}