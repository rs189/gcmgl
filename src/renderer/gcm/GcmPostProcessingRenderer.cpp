//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GcmPostProcessingRenderer.h"

#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <string.h>
#include "rsxutil/rsxutil.h"
#include "utils/Utils.h"
#include "utils/UtlMemory.h"

GcmPostProcessState_t CGcmPostProcessingRenderer::InitState(
	CRsxHeap& staticHeap)
{
	GcmPostProcessState_t state;
	memset(&state, 0, sizeof(state));

	CUtlVector<uint8> vertexProgramBinary = CUtils::ReadBinaryFile(
		"shaders/cg/post_desaturate.vpo");
	CUtlVector<uint8> fragmentProgramBinary = CUtils::ReadBinaryFile(
		"shaders/cg/post_desaturate.fpo");
	if (vertexProgramBinary.Count() == 0 || fragmentProgramBinary.Count() == 0)
	{
		return state;
	}

	state.m_pVertexProgramAligned = CUtlMemory::Alloc(
		uint32(vertexProgramBinary.Count()));
	if (!state.m_pVertexProgramAligned)
	{
		return state;
	}
	memcpy(
		state.m_pVertexProgramAligned,
		vertexProgramBinary.Base(),
		uint32(vertexProgramBinary.Count()));
	state.m_pVertexProgram = reinterpret_cast<const rsxVertexProgram*>(
		state.m_pVertexProgramAligned);
	uint32 vertexCodeSize;
	rsxVertexProgramGetUCode(
		const_cast<rsxVertexProgram*>(state.m_pVertexProgram),
		&state.m_pVertexProgramUCode,
		&vertexCodeSize);

	state.m_FragmentProgramAlloc = staticHeap.Alloc(
		uint32(fragmentProgramBinary.Count()),
		64);
	if (!state.m_FragmentProgramAlloc.m_pPtr)
	{
		return state;
	}
	state.m_pFragmentProgramAligned = state.m_FragmentProgramAlloc.m_pPtr;
	memcpy(
		state.m_pFragmentProgramAligned,
		fragmentProgramBinary.Base(),
		uint32(fragmentProgramBinary.Count()));
	state.m_pFragmentProgram = reinterpret_cast<const rsxFragmentProgram*>(
		state.m_pFragmentProgramAligned);
	void* pFragmentCode;
	uint32 fragmentCodeSize;
	rsxFragmentProgramGetUCode(
		const_cast<rsxFragmentProgram*>(state.m_pFragmentProgram),
		&pFragmentCode,
		&fragmentCodeSize);

	state.m_FragmentProgramBufferAlloc = staticHeap.Alloc(fragmentCodeSize, 64);
	if (!state.m_FragmentProgramBufferAlloc.m_pPtr)
	{
		return state;
	}
	state.m_pFragmentProgramBuffer = state.m_FragmentProgramBufferAlloc.m_pPtr;
	memcpy(state.m_pFragmentProgramBuffer, pFragmentCode, fragmentCodeSize);
	state.m_FragmentProgramOffset = state.m_FragmentProgramBufferAlloc.m_Offset;

	state.m_OffscreenColorAlloc = staticHeap.Alloc(
		display_height * color_pitch,
		64);
	state.m_OffscreenDepthAlloc = staticHeap.Alloc(
		display_height * depth_pitch,
		64);
	if (!state.m_OffscreenColorAlloc.m_pPtr || !state.m_OffscreenDepthAlloc.m_pPtr)
	{
		return state;
	}
	state.m_pOffscreenColor = state.m_OffscreenColorAlloc.m_pPtr;
	state.m_pOffscreenDepth = state.m_OffscreenDepthAlloc.m_pPtr;

	memset(state.m_pOffscreenColor, 0, display_height * color_pitch);
	memset(state.m_pOffscreenDepth, 0, display_height * depth_pitch);

	__sync_synchronize();

	state.m_OffscreenColorOffset = state.m_OffscreenColorAlloc.m_Offset;
	state.m_OffscreenDepthOffset = state.m_OffscreenDepthAlloc.m_Offset;

	state.m_QuadVerticesAlloc = staticHeap.Alloc(4 * 2 * sizeof(float32), 64);
	if (!state.m_QuadVerticesAlloc.m_pPtr)
	{
		return state;
	}
	state.m_pQuadVertices = state.m_QuadVerticesAlloc.m_pPtr;

	float32* pQuadVertices = reinterpret_cast<float32*>(state.m_pQuadVertices);
	pQuadVertices[0] = -1.0f;
	pQuadVertices[1] = -1.0f;
	pQuadVertices[2] = 1.0f;
	pQuadVertices[3] = -1.0f;
	pQuadVertices[4] = 1.0f;
	pQuadVertices[5] = 1.0f;
	pQuadVertices[6] = -1.0f;
	pQuadVertices[7] = 1.0f;

	__sync_synchronize();

	state.m_QuadVerticesOffset = state.m_QuadVerticesAlloc.m_Offset;

	return state;
}

void CGcmPostProcessingRenderer::ShutdownState(
	GcmPostProcessState_t& state,
	CRsxHeap& staticHeap)
{
	if (state.m_pVertexProgramAligned)
	{
		CUtlMemory::Free(state.m_pVertexProgramAligned);
		state.m_pVertexProgramAligned = GCMGL_NULL;
	}
	state.m_pVertexProgramUCode = GCMGL_NULL;
	if (state.m_pFragmentProgramAligned)
	{
		staticHeap.Free(state.m_FragmentProgramAlloc);
		state.m_pFragmentProgramAligned = GCMGL_NULL;
	}
	if (state.m_pFragmentProgramBuffer)
	{
		staticHeap.Free(state.m_FragmentProgramBufferAlloc);
		state.m_pFragmentProgramBuffer = GCMGL_NULL;
	}
	if (state.m_pOffscreenColor)
	{
		staticHeap.Free(state.m_OffscreenColorAlloc);
		state.m_pOffscreenColor = GCMGL_NULL;
	}
	if (state.m_pOffscreenDepth)
	{
		staticHeap.Free(state.m_OffscreenDepthAlloc);
		state.m_pOffscreenDepth = GCMGL_NULL;
	}
	if (state.m_pQuadVertices)
	{
		staticHeap.Free(state.m_QuadVerticesAlloc);
		state.m_pQuadVertices = GCMGL_NULL;
	}
	state.m_pVertexProgram = GCMGL_NULL;
}

void CGcmPostProcessingRenderer::Begin(const GcmPostProcessState_t& state)
{
	gcmSurface surface;
	surface.colorFormat = GCM_SURFACE_X8R8G8B8;
	surface.colorTarget = GCM_SURFACE_TARGET_0;
	surface.colorLocation[0] = GCM_LOCATION_RSX;
	surface.colorOffset[0] = state.m_OffscreenColorOffset;
	surface.colorPitch[0] = color_pitch;
	surface.colorLocation[1] = GCM_LOCATION_RSX;
	surface.colorLocation[2] = GCM_LOCATION_RSX;
	surface.colorLocation[3] = GCM_LOCATION_RSX;
	surface.colorOffset[1] = 0;
	surface.colorOffset[2] = 0;
	surface.colorOffset[3] = 0;
	surface.colorPitch[1] = color_pitch;
	surface.colorPitch[2] = color_pitch;
	surface.colorPitch[3] = color_pitch;
	surface.depthFormat = GCM_SURFACE_ZETA_Z24S8;
	surface.depthLocation = GCM_LOCATION_RSX;
	surface.depthOffset = depth_offset;
	surface.depthPitch = depth_pitch;
	surface.type = GCM_SURFACE_TYPE_LINEAR;
	surface.antiAlias = GCM_SURFACE_CENTER_1;
	surface.width = display_width;
	surface.height = display_height;
	surface.x = 0;
	surface.y = 0;
	rsxSetSurface(context, &surface);
}

void CGcmPostProcessingRenderer::End(const GcmPostProcessState_t& state)
{
	rsxSetDepthTestEnable(context, GCM_FALSE);
	rsxSetDepthWriteEnable(context, GCM_FALSE);
	rsxSetCullFaceEnable(context, GCM_FALSE);

	setRenderTarget(curr_fb);

	// Unbind all vertex attributes to avoid mismatch with post-process shader
	for (uint32 i = 0; i < 16; i++)
	{
		rsxBindVertexArrayAttrib(
			context, i, 0, 0, 0, 0,
			GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
	}

	rsxLoadVertexProgram(
		context,
		const_cast<rsxVertexProgram*>(state.m_pVertexProgram),
		state.m_pVertexProgramUCode);
	rsxLoadFragmentProgramLocation(
		context,
		const_cast<rsxFragmentProgram*>(state.m_pFragmentProgram),
		state.m_FragmentProgramOffset,
		GCM_LOCATION_RSX);

	gcmTexture texture;
	memset(&texture, 0, sizeof(gcmTexture));
	texture.format = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
	texture.mipmap = 1;
	texture.dimension = GCM_TEXTURE_DIMS_2D;
	texture.cubemap = GCM_FALSE;
	texture.remap =
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT);
	texture.width = display_width;
	texture.height = display_height;
	texture.depth = 1;
	texture.location = GCM_LOCATION_RSX;
	texture.pitch = color_pitch;
	texture.offset = state.m_OffscreenColorOffset;
	rsxLoadTexture(context, 0, &texture);
	rsxTextureControl(context, 0, GCM_TRUE, 0, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
	rsxTextureFilter(
		context, 0, 0,
		GCM_TEXTURE_NEAREST, GCM_TEXTURE_NEAREST,
		GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	rsxTextureWrapMode(
		context, 0,
		GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
		GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

	rsxBindVertexArrayAttrib(
		context, GCM_VERTEX_ATTRIB_POS, 0,
		state.m_QuadVerticesOffset, 8, 2,
		GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
	rsxInvalidateVertexCache(context);
	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);

	rsxSetFrequencyDividerOperation(context, GCM_FREQUENCY_MODULO);

	rsxDrawVertexArray(context, GCM_TYPE_QUADS, 0, 4);

	rsxSetDepthTestEnable(context, GCM_TRUE);
	rsxSetDepthWriteEnable(context, GCM_TRUE);
	rsxSetCullFaceEnable(context, GCM_TRUE);
}