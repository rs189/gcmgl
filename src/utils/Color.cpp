//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Color.h"

const CColor CColor::Black(0.0f, 0.0f, 0.0f, 1.0f);
const CColor CColor::White(1.0f, 1.0f, 1.0f, 1.0f);
const CColor CColor::Red(1.0f, 0.0f, 0.0f, 1.0f);
const CColor CColor::Green(0.0f, 1.0f, 0.0f, 1.0f);
const CColor CColor::Blue(0.0f, 0.0f, 1.0f, 1.0f);

// Packs a color into a 32-bit integer
uint32 CColor::PackColor(const CColor& color)
{
#ifdef PLATFORM_PS3
	// Big-endian
	return PackRGBA(color);
#else
	// Little-endian
	return PackABGR(color);
#endif
}

uint32 CColor::PackRGBA(const CColor& color)
{
	uint8 r8 = uint8(color.m_R * 255.0f);
	uint8 g8 = uint8(color.m_G * 255.0f);
	uint8 b8 = uint8(color.m_B * 255.0f);
	uint8 a8 = uint8(color.m_A * 255.0f);
	
	return (uint32(r8) << 24) | (uint32(g8) << 16) | (uint32(b8) << 8) | uint32(a8);
}

uint32 CColor::PackARGB(const CColor& color)
{
	uint8 r8 = uint8(color.m_R * 255.0f);
	uint8 g8 = uint8(color.m_G * 255.0f);
	uint8 b8 = uint8(color.m_B * 255.0f);
	uint8 a8 = uint8(color.m_A * 255.0f);

	return (uint32(a8) << 24) | (uint32(r8) << 16) | (uint32(g8) << 8) | uint32(b8);
}

uint32 CColor::PackABGR(const CColor& color)
{
	uint8 r8 = uint8(color.m_R * 255.0f);
	uint8 g8 = uint8(color.m_G * 255.0f);
	uint8 b8 = uint8(color.m_B * 255.0f);
	uint8 a8 = uint8(color.m_A * 255.0f);

	return (uint32(a8) << 24) | (uint32(b8) << 16) | (uint32(g8) << 8) | uint32(r8);
}