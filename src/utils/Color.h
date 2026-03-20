//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef COLOR_H
#define COLOR_H

#pragma once

#include "tier0/platform.h"

class CColor
{
public:
	CColor() :
		m_R(0.0f),
		m_G(0.0f),
		m_B(0.0f),
		m_A(1.0f)
	{
	}

	CColor(float32 r, float32 g, float32 b, float32 a = 1.0f) :
		m_R(r),
		m_G(g),
		m_B(b),
		m_A(a)
	{
	}

	static const CColor Black;
	static const CColor White;
	static const CColor Red;
	static const CColor Green;
	static const CColor Blue;

	static uint32 PackColor(const CColor& color);
	static uint32 PackRGBA(const CColor& color);
	static uint32 PackARGB(const CColor& color);
	static uint32 PackABGR(const CColor& color);
public:
	float32 m_R;
	float32 m_G;
	float32 m_B;
	float32 m_A;
};

#endif // COLOR_H