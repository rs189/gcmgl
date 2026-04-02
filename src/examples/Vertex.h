//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef VERTEX_H
#define VERTEX_H

#pragma once

#include "mathsfury/Vector3.h"
#include "utils/Color.h"

struct Vertex_t
{
	CVector3 m_Position; // 12 bytes
	uint32 m_Color; // 4 bytes (RGBA8 packed)

	Vertex_t() :
		m_Position(0.0f, 0.0f, 0.0f),
		m_Color(0)
	{
	}

	Vertex_t(const CVector3& position, uint32 color) :
		m_Position(position),
		m_Color(color)
	{
	}

	static uint32 PackColor(float32 r, float32 g, float32 b, float32 a = 1.0f)
	{
		return CColor::PackColor(CColor(r, g, b, a));
	}
};

#endif // VERTEX_H