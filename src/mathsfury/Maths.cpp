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
#else
#include <simde/x86/sse.h>
#endif
#endif

const float32 CMaths::PI = 3.14159265358979323846f;
const float32 CMaths::TwoPI = PI * 2.0f;
const float32 CMaths::Deg2Rad = PI / 180.0f;
const float32 CMaths::Rad2Deg = 180.0f / PI;

CVector3 CMaths::Normalise(const CVector3& v)
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float vec = { v.m_X, v.m_Y, v.m_Z, 0.0f };

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(vec, vec, zero);

	float32 lenSq = u.f[0] + u.f[1] + u.f[2];
	if (lenSq > 1e-8f)
	{
		vector float invLenVec = vec_splats(1.0f / Sqrt(lenSq));
		u.v = vec_madd(vec, invLenVec, zero);

		return CVector3(u.f[0], u.f[1], u.f[2]);
	}

	return CVector3(0.0f, 0.0f, 0.0f);
#else // PLATFORM_PS3
	float32 lenSq = v.m_X * v.m_X + v.m_Y * v.m_Y + v.m_Z * v.m_Z;
	if (lenSq > 1e-8f)
	{
		float32 invLen = 1.0f / Sqrt(lenSq);

		return CVector3(v.m_X * invLen, v.m_Y * invLen, v.m_Z * invLen);
	}

	return CVector3(0.0f, 0.0f, 0.0f);
#endif // !PLATFORM_PS3
#else // SIMD_ENABLED
	float32 lenSq = v.m_X * v.m_X + v.m_Y * v.m_Y + v.m_Z * v.m_Z;
	if (lenSq > 1e-8f)
	{
		float32 invLen = 1.0f / Sqrt(lenSq);

		return CVector3(v.m_X * invLen, v.m_Y * invLen, v.m_Z * invLen);
	}

	return CVector3(0.0f, 0.0f, 0.0f);
#endif // !SIMD_ENABLED
}

CVector3 CMaths::Cross(const CVector3& a, const CVector3& b)
{
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float va = { a.m_X, a.m_Y, a.m_Z, 0.0f };
	vector float vb = { b.m_X, b.m_Y, b.m_Z, 0.0f };

	vector unsigned char permYZX = { 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3, 12, 13, 14, 15 };
	vector unsigned char permZXY = { 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15 };

	vector float ayzx = vec_perm(va, va, permYZX);
	vector float bzxy = vec_perm(vb, vb, permZXY);
	vector float azxy = vec_perm(va, va, permZXY);
	vector float byzx = vec_perm(vb, vb, permYZX);
	
	vector float prod1 = vec_madd(ayzx, bzxy, zero);
	vector float prod2 = vec_madd(azxy, byzx, zero);

	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_sub(prod1, prod2);
	
	return CVector3(u.f[0], u.f[1], u.f[2]);
#else // PLATFORM_PS3
	simde__m128 va = simde_mm_set_ps(0.0f, a.m_Z, a.m_Y, a.m_X);
	simde__m128 vb = simde_mm_set_ps(0.0f, b.m_Z, b.m_Y, b.m_X);

	simde__m128 ayzx = simde_mm_shuffle_ps(
		va,
		va,
		SIMDE_MM_SHUFFLE(3, 0, 2, 1));
	simde__m128 bzxy = simde_mm_shuffle_ps(
		vb, vb, SIMDE_MM_SHUFFLE(3, 1, 0, 2));
	simde__m128 azxy = simde_mm_shuffle_ps(
		va,
		va,
		SIMDE_MM_SHUFFLE(3, 1, 0, 2));
	simde__m128 byzx = simde_mm_shuffle_ps(
		vb,
		vb,
		SIMDE_MM_SHUFFLE(3, 0, 2, 1));
	
	simde__m128 res = simde_mm_sub_ps(
		simde_mm_mul_ps(ayzx, bzxy),
		simde_mm_mul_ps(azxy, byzx));
	float32 temp[4];
	simde_mm_storeu_ps(temp, res);

	return CVector3(temp[0], temp[1], temp[2]);
#endif // !PLATFORM_PS3
#else // SIMD_ENABLED
	return CVector3(
		a.m_Y * b.m_Z - a.m_Z * b.m_Y,
		a.m_Z * b.m_X - a.m_X * b.m_Z,
		a.m_X * b.m_Y - a.m_Y * b.m_X);
#endif // !SIMD_ENABLED
}

float32 CMaths::Dot(const CVector3& a, const CVector3& b)
{ 
#ifdef SIMD_ENABLED
#ifdef PLATFORM_PS3
	vector float zero = { 0.0f, 0.0f, 0.0f, 0.0f };
	vector float va = { a.m_X, a.m_Y, a.m_Z, 0.0f };
	vector float vb = { b.m_X, b.m_Y, b.m_Z, 0.0f };
	
	union
	{
		vector float v;
		float32 f[4];
	}
	u;
	u.v = vec_madd(va, vb, zero);
	
	return u.f[0] + u.f[1] + u.f[2];
#endif // PLATFORM_PS3
#endif // SIMD_ENABLED
	return a.m_X * b.m_X + a.m_Y * b.m_Y + a.m_Z * b.m_Z;
}

CMatrix4 CMaths::Perspective(
	float32 fovyDeg,
	float32 aspect,
	float32 nearClip,
	float32 farClip)
{
	Assert(Abs(aspect) > 1e-6f);
	Assert(Abs(nearClip - farClip) > 1e-6f);
	Assert(fovyDeg > 0.0f && fovyDeg < 180.0f);

	float32 fovy = fovyDeg * Deg2Rad;
	float32 halfFovy = fovy * 0.5f;
	float32 f = 1.0f / Tan(halfFovy);

	float32 fOverAspect = f / aspect;
	float32 nearMinusFar = nearClip - farClip;
	float32 farPlusNear = farClip + nearClip;
	float32 twoFarNear = 2.0f * farClip * nearClip;

	float32 zTerm = farPlusNear / nearMinusFar;
	float32 wTerm = twoFarNear / nearMinusFar;
	
	CMatrix4 result;
	result.m_Data[0] = fOverAspect;
	result.m_Data[1] = 0.0f;
	result.m_Data[2] = 0.0f;
	result.m_Data[3] = 0.0f;
	
	result.m_Data[4] = 0.0f;
	result.m_Data[5] = f;
	result.m_Data[6] = 0.0f;
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = 0.0f;
	result.m_Data[9] = 0.0f;
	result.m_Data[10] = zTerm;
	result.m_Data[11] = -1.0f;

	result.m_Data[12] = 0.0f;
	result.m_Data[13] = 0.0f;
	result.m_Data[14] = wTerm;
	result.m_Data[15] = 0.0f;

	return result;
}

CMatrix4 CMaths::Orthographic(
	float32 left,
	float32 right,
	float32 bottom,
	float32 top,
	float32 nearClip,
	float32 farClip)
{
	float32 width = right - left;
	float32 height = top - bottom;
	float32 depth = farClip - nearClip;

	CMatrix4 result;
	result.m_Data[0] = 2.0f / width;
	result.m_Data[1] = 0.0f;
	result.m_Data[2] = 0.0f;
	result.m_Data[3] = 0.0f;

	result.m_Data[4] = 0.0f;
	result.m_Data[5] = 2.0f / height;
	result.m_Data[6] = 0.0f;
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = 0.0f;
	result.m_Data[9] = 0.0f;
	result.m_Data[10] = -2.0f / depth;
	result.m_Data[11] = 0.0f;

	result.m_Data[12] = -(right + left) / width;
	result.m_Data[13] = -(top + bottom) / height;
	result.m_Data[14] = -(farClip + nearClip) / depth;
	result.m_Data[15] = 1.0f;

	return result;
}

CMatrix4 CMaths::LookAt(
	const CVector3& eye,
	const CVector3& target,
	const CVector3& up)
{
	CVector3 f = Normalise(target - eye);

	float32 fDotUp = Dot(f, up);
	Assert(fDotUp < 0.9999f && fDotUp > -0.9999f);
	
	CVector3 s = Normalise(Cross(f, up));
	CVector3 u = Cross(s, f);
	
	CMatrix4 result;
	result.m_Data[0] = s.m_X;
	result.m_Data[1] = u.m_X;
	result.m_Data[2] = -f.m_X;
	result.m_Data[3] = 0.0f;
	
	result.m_Data[4] = s.m_Y;
	result.m_Data[5] = u.m_Y;
	result.m_Data[6] = -f.m_Y;
	result.m_Data[7] = 0.0f;

	result.m_Data[8] = s.m_Z;
	result.m_Data[9] = u.m_Z;
	result.m_Data[10] = -f.m_Z;
	result.m_Data[11] = 0.0f;

	result.m_Data[12] = -Dot(s, eye);
	result.m_Data[13] = -Dot(u, eye);
	result.m_Data[14] = Dot(f, eye);
	result.m_Data[15] = 1.0f;

	return result;
}

CMatrix4 CMaths::Translate(const CMatrix4& m, const CVector3& v)
{
	CMatrix4 result = m;
	result.m_Data[12] += 
		m.m_Data[0] * v.m_X + m.m_Data[4] * v.m_Y + m.m_Data[8] * v.m_Z;
	result.m_Data[13] +=
		m.m_Data[1] * v.m_X + m.m_Data[5] * v.m_Y + m.m_Data[9] * v.m_Z;
	result.m_Data[14] +=
		m.m_Data[2] * v.m_X + m.m_Data[6] * v.m_Y + m.m_Data[10] * v.m_Z;

	return result;
}

CMatrix4 CMaths::Rotate(
	const CMatrix4& m,
	float32 angleDeg,
	const CVector3& axis)
{
	float32 a = angleDeg * Deg2Rad;
	float32 c = Cos(a);
	float32 s = Sin(a);
	CVector3 ax = Normalise(axis);
	
	float32 oneMinusC = 1.0f - c;
	float32 axAx = ax.m_X * ax.m_X;
	float32 ayAy = ax.m_Y * ax.m_Y;
	float32 azAz = ax.m_Z * ax.m_Z;
	float32 axAy = ax.m_X * ax.m_Y;
	float32 axAz = ax.m_X * ax.m_Z;
	float32 ayAz = ax.m_Y * ax.m_Z;

	float32 r00 = c + oneMinusC * axAx;
	float32 r01 = oneMinusC * axAy + s * ax.m_Z;
	float32 r02 = oneMinusC * axAz - s * ax.m_Y;
	
	float32 r10 = oneMinusC * axAy - s * ax.m_Z;
	float32 r11 = c + oneMinusC * ayAy;
	float32 r12 = oneMinusC * ayAz + s * ax.m_X;
	
	float32 r20 = oneMinusC * axAz + s * ax.m_Y;
	float32 r21 = oneMinusC * ayAz - s * ax.m_X;
	float32 r22 = c + oneMinusC * azAz;

	CMatrix4 result;
	result.m_Data[0] =
		m.m_Data[0] * r00 + m.m_Data[4] * r10 + m.m_Data[8] * r20;
	result.m_Data[1] =
		m.m_Data[1] * r00 + m.m_Data[5] * r10 + m.m_Data[9] * r20;
	result.m_Data[2] =
		m.m_Data[2] * r00 + m.m_Data[6] * r10 + m.m_Data[10] * r20;
	result.m_Data[3] = m.m_Data[3];
	
	result.m_Data[4] =
		m.m_Data[0] * r01 + m.m_Data[4] * r11 + m.m_Data[8] * r21;
	result.m_Data[5] =
		m.m_Data[1] * r01 + m.m_Data[5] * r11 + m.m_Data[9] * r21;
	result.m_Data[6] =
		m.m_Data[2] * r01 + m.m_Data[6] * r11 + m.m_Data[10] * r21;
	result.m_Data[7] = m.m_Data[7];
	
	result.m_Data[8] =
		m.m_Data[0] * r02 + m.m_Data[4] * r12 + m.m_Data[8] * r22;
	result.m_Data[9] =
		m.m_Data[1] * r02 + m.m_Data[5] * r12 + m.m_Data[9] * r22;
	result.m_Data[10] =
		m.m_Data[2] * r02 + m.m_Data[6] * r12 + m.m_Data[10] * r22;
	result.m_Data[11] = m.m_Data[11];
	
	result.m_Data[12] = m.m_Data[12];
	result.m_Data[13] = m.m_Data[13];
	result.m_Data[14] = m.m_Data[14];
	result.m_Data[15] = m.m_Data[15];

	return result;
}

CMatrix4 CMaths::Scale(const CMatrix4& m, const CVector3& v)
{
	CMatrix4 result;
	result.m_Data[0] = m.m_Data[0] * v.m_X;
	result.m_Data[1] = m.m_Data[1] * v.m_X;
	result.m_Data[2] = m.m_Data[2] * v.m_X;
	result.m_Data[3] = m.m_Data[3];

	result.m_Data[4] = m.m_Data[4] * v.m_Y;
	result.m_Data[5] = m.m_Data[5] * v.m_Y;
	result.m_Data[6] = m.m_Data[6] * v.m_Y;
	result.m_Data[7] = m.m_Data[7];

	result.m_Data[8] = m.m_Data[8] * v.m_Z;
	result.m_Data[9] = m.m_Data[9] * v.m_Z;
	result.m_Data[10] = m.m_Data[10] * v.m_Z;
	result.m_Data[11] = m.m_Data[11];
	
	result.m_Data[12] = m.m_Data[12];
	result.m_Data[13] = m.m_Data[13];
	result.m_Data[14] = m.m_Data[14];
	result.m_Data[15] = m.m_Data[15];

	return result;
}

EquirectUV_t CMaths::DirectionToEquirect(const CVector3& dir)
{
	CVector3 d = Normalise(dir);
	float32 lon = ATan2(d.m_X, d.m_Z);
	float32 clampedY = Clamp(d.m_Y, -1.0f, 1.0f);
	float32 lat = ASin(clampedY);

	float32 u = (lon + PI) / (2.0f * PI);
	float32 v = (lat + (PI * 0.5f)) / PI;

	EquirectUV_t result;
	result.m_U = u;
	result.m_V = v;

	return result;
}