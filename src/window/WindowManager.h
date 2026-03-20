//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#pragma once

#include "tier0/platform.h"
#include "Window.h"

enum WindowBackend_t
{
	WindowBackendGlfw = 0,
	WindowBackendPs3 = 1,
	WindowBackendCount
};

class CWindowManager
{
public:
	CWindowManager();
	~CWindowManager();

	bool Start(
		WindowBackend_t windowBackend,
		const WindowConfig_t& windowConfig);
	void Shutdown();

	IWindow* GetWindow();
private:
	WindowBackend_t m_WindowBackend;
	WindowConfig_t m_WindowConfig;
	IWindow* m_pWindow;
};

#endif // WINDOW_MANAGER_H