//===----------------------------------------------------------------------===//
//
// Part of the mathsfurry, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef QUATERNION_H
#define QUATERNION_H

#pragma once

#include "tier0/platform.h"

class CMatrix4;
class CVector3;

class CQuaternion
{
public:
	float32 m_X;
	float32 m_Y;
	float32 m_Z;
	float32 m_W;

	CQuaternion();
	CQuaternion(float32 x, float32 y, float32 z, float32 w);

	CQuaternion operator*(const CQuaternion& other) const;

	CMatrix4 ToMatrix() const;
	CMatrix4 ToTransformMatrix(
		const CVector3& position,
		const CVector3& scale
	) const;
	CQuaternion& Normalise();
	float32 Length() const;

	static CQuaternion FromEuler(float32 pitch, float32 yaw, float32 roll);
};

#endif // QUATERNION_H