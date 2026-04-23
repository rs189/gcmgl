//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GCM_POST_PROCESSING_RENDERER_H
#define GCM_POST_PROCESSING_RENDERER_H

#pragma once

#include <rsx/rsx.h>
#include "tier0/platform.h"
#include "utils/RsxHeap.h"

struct GcmPostProcessState_t
{
	uint32 m_FragmentProgramOffset;
	uint32 m_OffscreenColorOffset;
	uint32 m_OffscreenDepthOffset;
	uint32 m_QuadVerticesOffset;
	void* m_pVertexProgramAligned;
	void* m_pVertexProgramUCode;
	void* m_pFragmentProgramAligned;
	void* m_pFragmentProgramBuffer;
	void* m_pOffscreenColor;
	void* m_pOffscreenDepth;
	void* m_pQuadVertices;
	const rsxVertexProgram* m_pVertexProgram;
	const rsxFragmentProgram* m_pFragmentProgram;
	RsxAllocation_t m_FragmentProgramAlloc;
	RsxAllocation_t m_FragmentProgramBufferAlloc;
	RsxAllocation_t m_OffscreenColorAlloc;
	RsxAllocation_t m_OffscreenDepthAlloc;
	RsxAllocation_t m_QuadVerticesAlloc;
};

class CGcmPostProcessingRenderer
{
public:
	static GcmPostProcessState_t InitState(CRsxHeap& staticHeap);
	static void ShutdownState(
		GcmPostProcessState_t& state,
		CRsxHeap& staticHeap);
	static void Begin(const GcmPostProcessState_t& state);
	static void End(const GcmPostProcessState_t& state);
};

#endif // GCM_POST_PROCESSING_RENDERER_H