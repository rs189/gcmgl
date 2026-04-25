//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef WINDOW_H
#define WINDOW_H

#pragma once

#include "tier0/platform.h"
#include "utils/FixedString.h"

struct WindowConfig_t
{
	int32 m_Width;
	int32 m_Height;
	float32 m_AspectRatio;
	CFixedString m_Title;

	WindowConfig_t() :
		m_Width(1280),
		m_Height(720),
		m_AspectRatio(float32(m_Width) / float32(m_Height)),
		m_Title("gcmgl")
	{
	}
};

class IWindow
{
public:
	virtual ~IWindow()
	{
	}

	virtual bool Init(const WindowConfig_t& windowConfig) = 0;
	virtual void Shutdown() = 0;

	virtual void PollEvents() = 0;
	virtual void MakeContextCurrent() = 0;
	virtual bool ShouldClose() = 0;
	virtual void* GetNativeHandle() = 0;
	virtual void GetSize(uint32& width, uint32& height) const = 0;
	virtual void GetFramebufferSize(uint32& width, uint32& height) const = 0;
};

IWindow* CreateWindowInstance();
void DestroyWindowInstance(IWindow* pWindow);

#endif // WINDOW_H