//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "WindowGlfw.h"
#include "tier0/dbg.h"
#include "utils/UtlMemory.h"

CWindowGlfw::CWindowGlfw() :
	m_pWindow(GCMGL_NULL)
{
}

CWindowGlfw::~CWindowGlfw()
{
	Shutdown();
}

bool CWindowGlfw::Init(const WindowConfig_t& windowConfig)
{
	if (!glfwInit())
	{
		Error("[CWindowGlfw] Failed to initialize GLFW\n");
		
		return false;
	}

	// Configuration Table
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);

	m_pWindow = glfwCreateWindow(
		windowConfig.m_Width,
		windowConfig.m_Height,
		windowConfig.m_Title.Get(),
		GCMGL_NULL,
		GCMGL_NULL);
	if (m_pWindow == GCMGL_NULL)
	{
		Error("[CWindowGlfw] Failed to create OpenGL 3.2 context\n");

		return false;
	}

	glfwMakeContextCurrent(m_pWindow);

	return true;
}

void CWindowGlfw::Shutdown()
{
	if (m_pWindow != GCMGL_NULL)
	{
		glfwDestroyWindow(m_pWindow);
		m_pWindow = GCMGL_NULL;
	}

	glfwTerminate();
}

void CWindowGlfw::PollEvents()
{
	glfwPollEvents();
}

void CWindowGlfw::MakeContextCurrent()
{
	if (m_pWindow != GCMGL_NULL)
	{
		glfwMakeContextCurrent(m_pWindow);
	}
}

bool CWindowGlfw::ShouldClose()
{
	if (m_pWindow == GCMGL_NULL)
	{
		return true;
	}

	return glfwWindowShouldClose(m_pWindow) != 0;
}

void* CWindowGlfw::GetNativeHandle()
{
	return reinterpret_cast<void*>(m_pWindow);
}

IWindow* CreateWindowInstance()
{
	void* pMemory = CUtlMemory::Alloc(sizeof(CWindowGlfw));
	if (pMemory)
	{
		return new(pMemory) CWindowGlfw();
	}

	return GCMGL_NULL;
}

void DestroyWindowInstance(IWindow* pWindow)
{
	if (pWindow)
	{
		pWindow->~IWindow();
		CUtlMemory::Free(pWindow);
	}
}