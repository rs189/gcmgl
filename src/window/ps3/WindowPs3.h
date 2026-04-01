//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef WINDOW_PS3_H
#define WINDOW_PS3_H

#pragma once

#include "../Window.h"
#include <sysutil/sysutil.h>

class CWindowPs3 : public IWindow
{
public:
	CWindowPs3();
	virtual ~CWindowPs3() GCMGL_OVERRIDE;

	virtual bool Init(const WindowConfig_t& windowConfig) GCMGL_OVERRIDE;
	virtual void Shutdown() GCMGL_OVERRIDE;

	virtual void PollEvents() GCMGL_OVERRIDE;
	virtual void MakeContextCurrent() GCMGL_OVERRIDE;
	virtual bool ShouldClose() GCMGL_OVERRIDE;
	virtual void* GetNativeHandle() GCMGL_OVERRIDE;
private:
	bool m_IsRunning;

	static void SysutilExitCallback(
		u64 status,
		u64 param,
		void* pUserData);
};

void DestroyWindowInstance(IWindow* pWindow);

#endif // WINDOW_PS3_H