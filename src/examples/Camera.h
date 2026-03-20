//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef CAMERA_H
#define CAMERA_H

#pragma once

#include "mathsfury/Vector3.h"
#include "mathsfury/Matrix4.h"
#include "mathsfury/Maths.h"

class CCamera
{
public:
	CCamera() :
		m_Position(0.0f, 0.0f, 5.0f),
		m_Yaw(0.0f),
		m_Pitch(0.0f)
	{
	}

	CMatrix4 GetViewMatrix() const
	{
		return CMaths::LookAt(
			m_Position,
			CVector3(0.0f, 0.0f, 0.0f),
			CVector3(0.0f, 1.0f, 0.0f));
	}
	
	CMatrix4 GetProjectionMatrix(float32 aspect) const
	{
		return CMaths::Perspective(45.0f, aspect, 0.1f, 20000.0f);
	}

public:
	CVector3 m_Position;
	float32 m_Yaw;
	float32 m_Pitch;
};

#endif // CAMERA_H