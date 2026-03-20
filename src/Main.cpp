//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
#include "tier0/dbg.h"
#include "window/WindowManager.h"
#include "window/Window.h"
#include "renderer/Renderer.h"
#include "utils/Time.h"

#ifdef GCMGL_DIAGNOSTICS
#include "utils/DiagnosticsReporter.h"
#endif

#ifdef PLATFORM_PS3
#include "rsxutil/rsxutil.h"
#endif

int RunTriangleExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunCubeExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunShaderExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunTexturedExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunLitExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunTexturedLitExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);
int RunBatchExample(
	class CWindowManager& windowManager,
	class IRenderer* pRenderer,
	class IWindow* pWnd,
	const struct WindowConfig_t& windowConfig);

int main(int argc, char* argv[])
{
	int32 result = 0;

	WindowConfig_t windowConfig;
	windowConfig.m_Width = 1280;
	windowConfig.m_Height = 720;
	windowConfig.m_AspectRatio = float32(windowConfig.m_Width) / float32(windowConfig.m_Height);
	windowConfig.m_Title = "gcmgl";

	CWindowManager windowManager;
#ifdef PLATFORM_PS3
	if (!windowManager.Start(WindowBackendPs3, windowConfig))
#else
	if (!windowManager.Start(WindowBackendGlfw, windowConfig))
#endif
	{
		Error("[ERROR][Game] Failed to start window manager\n");

		return 1;
	}

	IWindow* pWindow = windowManager.GetWindow();

	IRenderer* pRenderer = CreateRenderer();
	if (!pRenderer)
	{
		Error("[ERROR][Game] Failed to create renderer\n");

		return 1;
	}

	RendererDesc_t rendererDesc = {
		uint32(windowConfig.m_Width),
		uint32(windowConfig.m_Height),
		false,
		true,
		pWindow->GetNativeHandle()
	};
	if (!pRenderer->Init(rendererDesc))
	{
		Error("[ERROR][Game] Failed to initialize renderer\n");

		return 1;
	}

#ifdef PLATFORM_PS3
	windowConfig.m_Width = int32(display_width);
	windowConfig.m_Height = int32(display_height);
	windowConfig.m_AspectRatio = aspect_ratio;
#endif

	CTime::Initialize();

#ifdef GCMGL_DIAGNOSTICS
	CDiagnosticsReporter::Initialize();
#endif

#if defined(EXAMPLE_TRIANGLE)
	result = RunTriangleExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_CUBE)
	result = RunCubeExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_SHADER)
	result = RunShaderExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_TEXTURED)
	result = RunTexturedExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_LIT)
	result = RunLitExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_TEXTURED_LIT)
	result = RunTexturedLitExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#elif defined(EXAMPLE_BATCH)
	result = RunBatchExample(
		windowManager,
		pRenderer,
		pWindow,
		windowConfig);
#endif

	pRenderer->Shutdown();
	DestroyRenderer(pRenderer);
	pRenderer = GCMGL_NULL;

#ifdef GCMGL_DIAGNOSTICS
	CDiagnosticsReporter::Shutdown();
#endif

	return result;
}