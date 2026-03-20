//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "WindowPs3.h"
#include "tier0/dbg.h"
#include "utils/UtlMemory.h"
#include <sys/process.h>
#include <sysutil/video.h>
#include <io/pad.h>

CWindowPs3::CWindowPs3() :
	m_IsRunning(false)
{
}

CWindowPs3::~CWindowPs3()
{
	Shutdown();
}

bool CWindowPs3::Init(const WindowConfig_t& windowConfig)
{
	if (ioPadInit(7) != 0)
	{
		Error("[CWindowPs3] Failed to initialize pad system\n");
		
		return false;
	}

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, SysutilExitCallback, this);

	m_IsRunning = true;

	return true;
}

void CWindowPs3::Shutdown()
{
	m_IsRunning = false;

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	ioPadEnd();
}

void CWindowPs3::PollEvents()
{
	sysUtilCheckCallback();
}

void CWindowPs3::MakeContextCurrent()
{
	Msg("[CWindowPs3] MakeContextCurrent() stub\n");
}

bool CWindowPs3::ShouldClose()
{
	return !m_IsRunning;
}

void* CWindowPs3::GetNativeHandle()
{
	Msg("[CWindowPs3] GetNativeHandle() stub\n");

	return GCMGL_NULL;
}

void CWindowPs3::SysutilExitCallback(
	unsigned long status,
	unsigned long param,
	void* pUserData)
{
	CWindowPs3* pWindow = reinterpret_cast<CWindowPs3*>(pUserData);
	Msg(
		"[CWindowPs3] SysutilExitCallback triggered with status=0x%lx, param=0x%lx\n",
		status,
		param);

	switch (status)
	{
		case SYSUTIL_EXIT_GAME:
		{
			Msg("[CWindowPs3] SYSUTIL_EXIT_GAME status received\n");
			if (pWindow != GCMGL_NULL)
			{
				pWindow->m_IsRunning = false;
			}

			break;
		}
		case SYSUTIL_DRAW_BEGIN:
		{
			Msg("[CWindowPs3] SYSUTIL_DRAW_BEGIN received\n");

			break;
		}
		case SYSUTIL_DRAW_END:
		{
			Msg("[CWindowPs3] SYSUTIL_DRAW_END received\n");

			break;
		}
		default:
		{
			Warning("[CWindowPs3] Unknown status received: 0x%lx\n", status);

			break;
		}
	}
}

IWindow* CreateWindowInstance()
{
	void* pMemory = CUtlMemory::Alloc(sizeof(CWindowPs3));
	if (pMemory)
	{
		return new(pMemory) CWindowPs3();
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