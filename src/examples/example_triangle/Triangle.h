//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef EXAMPLE_TRIANGLE_H
#define EXAMPLE_TRIANGLE_H

#pragma once

#include "window/WindowManager.h"
#include "window/Window.h"
#include "renderer/Renderer.h"

int32 RunTriangleExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig);

#endif // EXAMPLE_TRIANGLE_H