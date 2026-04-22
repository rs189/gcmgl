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
#include "renderer/gcm/GcmRenderer.h"
#endif

#ifdef EXAMPLE_TRIANGLE
#include "examples/example_triangle/Triangle.h"
#elif defined(EXAMPLE_CUBE)
#include "examples/example_cube/Cube.h"
#elif defined(EXAMPLE_SHADER)
#include "examples/example_shader/Shader.h"
#elif defined(EXAMPLE_TEXTURED)
#include "examples/example_textured/Textured.h"
#elif defined(EXAMPLE_LIT)
#include "examples/example_lit/Lit.h"
#elif defined(EXAMPLE_TEXTURED_LIT)
#include "examples/example_texturedLit/TexturedLit.h"
#elif defined(EXAMPLE_BATCH)
#include "examples/example_batch/Batch.h"
#elif defined(EXAMPLE_BATCH_INSTANCED)
#include "examples/example_batchInstanced/BatchInstanced.h"
#endif

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
#else // PLATFORM_PS3
	if (!windowManager.Start(WindowBackendGlfw, windowConfig))
#endif // !PLATFORM_PS3
	{
		Error("[Game] Failed to start window manager\n");

		return 1;
	}

	IWindow* pWindow = windowManager.GetWindow();

	IRenderer* pRenderer = CreateRenderer();
	if (!pRenderer)
	{
		Error("[Game] Failed to create renderer\n");

		return 1;
	}

	RendererDesc_t rendererDesc = {
		pWindow->GetNativeHandle(),
		uint32(windowConfig.m_Width),
		uint32(windowConfig.m_Height),
		false,
		true
	};
	if (!pRenderer->Init(rendererDesc))
	{
		Error("[Game] Failed to initialize renderer\n");

		return 1;
	}

#ifdef PLATFORM_PS3
	windowConfig.m_Width = int32(display_width);
	windowConfig.m_Height = int32(display_height);
	windowConfig.m_AspectRatio = aspect_ratio;
#endif // PLATFORM_PS3

	CTime::Init();

#ifdef GCMGL_DIAGNOSTICS
	CDiagnosticsReporter::Init();
#endif // GCMGL_DIAGNOSTICS

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
#elif defined(EXAMPLE_BATCH_INSTANCED)
	result = RunBatchInstancedExample(
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
#endif // GCMGL_DIAGNOSTICS

	return result;
}