//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "WindowManager.h"
#include "tier0/dbg.h"

CWindowManager::CWindowManager() :
	m_WindowBackend(WindowBackendGlfw),
	m_pWindow(GCMGL_NULL)
{
}

CWindowManager::~CWindowManager()
{
	Shutdown();
}

bool CWindowManager::Start(
	WindowBackend_t windowBackend,
	const WindowConfig_t& windowConfig)
{
	Shutdown();
	m_WindowBackend = windowBackend;

	m_pWindow = CreateWindowInstance();
	if (m_pWindow == GCMGL_NULL)
	{
		Error("[WindowManager] Failed to create window instance\n");
		
		return false;
	}

	if (!m_pWindow->Init(windowConfig))
	{
		Error("[WindowManager] Failed to initialize window\n");
		DestroyWindowInstance(m_pWindow);
		m_pWindow = GCMGL_NULL;

		return false;
	}

	m_WindowConfig = windowConfig;

	return true;
}

void CWindowManager::Shutdown()
{
	if (m_pWindow != GCMGL_NULL)
	{
		m_pWindow->Shutdown();
		DestroyWindowInstance(m_pWindow);
		m_pWindow = GCMGL_NULL;
	}
}

IWindow* CWindowManager::GetWindow()
{
	return m_pWindow;
}