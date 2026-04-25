//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef WINDOW_GLFW_H
#define WINDOW_GLFW_H

#pragma once

#include "../Window.h"
#include <GLFW/glfw3.h>

class CWindowGlfw : public IWindow
{
public:
	CWindowGlfw();
	virtual ~CWindowGlfw() GCMGL_OVERRIDE;

	virtual bool Init(const WindowConfig_t& windowConfig) GCMGL_OVERRIDE;
	virtual void Shutdown() GCMGL_OVERRIDE;

	virtual void PollEvents() GCMGL_OVERRIDE;
	virtual void MakeContextCurrent() GCMGL_OVERRIDE;
	virtual bool ShouldClose() GCMGL_OVERRIDE;
	virtual void* GetNativeHandle() GCMGL_OVERRIDE;
	virtual void GetSize(uint32& width, uint32& height) const GCMGL_OVERRIDE;
	virtual void GetFramebufferSize(
		uint32& width,
		uint32& height) const GCMGL_OVERRIDE;
private:
	GLFWwindow* m_pWindow;
};

void DestroyWindowInstance(IWindow* pWindow);

#endif // WINDOW_GLFW_H