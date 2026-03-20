//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef EXAMPLE_SHADER_H
#define EXAMPLE_SHADER_H

#pragma once

#include "window/WindowManager.h"
#include "window/Window.h"
#include "renderer/Renderer.h"

int32 RunShaderExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig);

#endif // EXAMPLE_SHADER_H