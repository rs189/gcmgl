//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef POST_PROCESSING_RENDERER_H
#define POST_PROCESSING_RENDERER_H

#pragma once

#include "Renderer.h"

class IPostProcessingRenderer
{
public:
	virtual ~IPostProcessingRenderer()
	{
	}

	virtual bool InitPostProcessing() = 0;
	virtual void BeginPostProcessing() = 0;
	virtual void EndPostProcessing() = 0;
	virtual void ShutdownPostProcessing() = 0;
};

class CPostProcessingRenderer : public virtual CRenderer, public IPostProcessingRenderer
{
public:
	CPostProcessingRenderer()
	{
	}
};

#endif // POST_PROCESSING_RENDERER_H