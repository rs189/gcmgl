//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GcmRenderer.h"
#include "GcmBatchRenderer.h"
#include "tier0/dbg.h"
#include "utils/Utils.h"
#include "utils/UtlMemory.h"
#include "mathsfury/Maths.h"
#include "mathsfury/Vector4.h"
#include <string.h>
#include <sys/memory.h>

#if defined(SIMD_ENABLED)
#include <altivec.h>
#undef pixel
#undef bool
#endif

#ifdef PS3_SPU_ENABLED
#include "platform/ps3/spu/SpuCommon.h"
#include "spu/SpuBatchTransformManager.h"
#endif

CGcmRenderer::CGcmRenderer() :
	m_pHostAddr(GCMGL_NULL),
	m_HostSize(0),
	m_StagingIndex(0)
#ifdef PS3_SPU_ENABLED
	,m_pSpuBtm(GCMGL_NULL)
#endif
{
	m_NextHandle = 1;

	m_PipelineState.m_hShaderProgram = 0;
	m_PipelineState.m_hVertexBuffer = 0;
	m_PipelineState.m_hIndexBuffer = 0;
	m_PipelineState.m_VertexOffset = 0;
	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_pVertexLayout = GCMGL_NULL;

	m_StateDirtyFlags = StateDirtyFlags_t::All;
}

CGcmRenderer::~CGcmRenderer()
{
	Shutdown();
}

bool CGcmRenderer::Init(const RendererDesc_t& rendererDesc)
{
	m_HostSize = 1024 * 1024 * 128; // 128MB
	sys_mem_addr_t hostAddr = 0;
	if (sysMemoryAllocate(m_HostSize, SYS_MEMORY_PAGE_SIZE_1M, &hostAddr) != 0)
	{
		Error("[GCMRenderer] Failed to allocate host memory\n");

		return false;
	}
	m_pHostAddr = reinterpret_cast<void*>(hostAddr);

	initScreen(m_pHostAddr, m_HostSize);

	rsxSetColorMask(
		context,
		GCM_COLOR_MASK_B |
		GCM_COLOR_MASK_G |
		GCM_COLOR_MASK_R |
		GCM_COLOR_MASK_A);

	rsxSetColorMaskMrt(context, 0);

	m_Viewport.m_X = 0.0f;
	m_Viewport.m_Y = 0.0f;
	m_Viewport.m_Width = float32(display_width);
	m_Viewport.m_Height = float32(display_height);
	m_Viewport.m_MinDepth = 0.0f;
	m_Viewport.m_MaxDepth = 1.0f;

	m_ViewportScale[0] = m_Viewport.m_Width * 0.5f;
	m_ViewportScale[1] = m_Viewport.m_Height * -0.5f;
	m_ViewportScale[2] = (m_Viewport.m_MaxDepth - m_Viewport.m_MinDepth) * 0.5f;
	m_ViewportScale[3] = 0.0f;
	m_ViewportOffset[0] = m_Viewport.m_X + m_Viewport.m_Width * 0.5f;
	m_ViewportOffset[1] = m_Viewport.m_Y + m_Viewport.m_Height * 0.5f;
	m_ViewportOffset[2] = (m_Viewport.m_MaxDepth + m_Viewport.m_MinDepth) * 0.5f;
	m_ViewportOffset[3] = 0.0f;

	rsxSetViewport(
		context,
		uint16(m_Viewport.m_X),
		uint16(m_Viewport.m_Y),
		uint16(m_Viewport.m_Width),
		uint16(m_Viewport.m_Height),
		m_Viewport.m_MinDepth,
		m_Viewport.m_MaxDepth,
		m_ViewportScale,
		m_ViewportOffset);
	rsxSetScissor(
		context,
		uint16(m_Viewport.m_X),
		uint16(m_Viewport.m_Y),
		uint16(m_Viewport.m_Width),
		uint16(m_Viewport.m_Height));

	rsxSetDepthTestEnable(context, GCM_TRUE);
	rsxSetDepthFunc(context, GCM_LESS);
	rsxSetShadeModel(context, GCM_SHADE_MODEL_SMOOTH);
	rsxSetDepthWriteEnable(context, 1);
	rsxSetFrontFace(context, GCM_FRONTFACE_CCW);

	rsxSetCullFaceEnable(context, GCM_TRUE);
	rsxSetCullFace(context, GCM_CULL_BACK);

	rsxSetZMinMaxControl(context, 0, 1, 1);

	for (int32 i = 0; i < 8; i++)
	{
		rsxSetViewportClip(
			context,
			i,
			display_width,
			display_height);
	}

	rsxSetUserClipPlaneControl(
		context,
		GCM_USER_CLIP_PLANE_DISABLE,
		GCM_USER_CLIP_PLANE_DISABLE,
		GCM_USER_CLIP_PLANE_DISABLE,
		GCM_USER_CLIP_PLANE_DISABLE,
		GCM_USER_CLIP_PLANE_DISABLE,
		GCM_USER_CLIP_PLANE_DISABLE);

	SetEnvironment();
	setRenderTarget(curr_fb);

#ifdef PS3_SPU_ENABLED
	void* pSpuBtmMemory = CUtlMemory::Alloc(sizeof(CSpuBatchTransformManager));
	if (pSpuBtmMemory)
	{
		m_pSpuBtm = new(pSpuBtmMemory) CSpuBatchTransformManager();
		if (!m_pSpuBtm->Initialize())
		{
			m_pSpuBtm->~CSpuBatchTransformManager();
			CUtlMemory::Free(pSpuBtmMemory);
			m_pSpuBtm = GCMGL_NULL;
		}
	}
#endif // PS3_SPU_ENABLED

	return true;
}

void CGcmRenderer::Shutdown()
{
	if (!m_pHostAddr) return;

#ifdef PS3_SPU_ENABLED
	if (m_pSpuBtm)
	{
		m_pSpuBtm->Shutdown();
		m_pSpuBtm->~CSpuBatchTransformManager();
		CUtlMemory::Free(m_pSpuBtm);
		m_pSpuBtm = GCMGL_NULL;
	}
#endif

	rsxFlushBuffer(context);
	waitFinish();

	for (int32 i = m_BufferResources.FirstInorder(); m_BufferResources.IsValidIndex(i); i = m_BufferResources.NextInorder(i))
	{
		BufferResource_t& bufferResource = m_BufferResources.Element(i);

		bool isStaging = false;
		for (int32 j = 0; j < 2; j++)
		{
			if (bufferResource.m_pPtr == m_StagingVertexBuffer[j].m_pPtr ||
				bufferResource.m_pPtr == m_StagingIndexBuffer[j].m_pPtr)
			{
				isStaging = true;

				break;
			}
		}

		if (isStaging)
		{
			continue;
		}

		if (bufferResource.m_pPtr)
		{
			rsxFree(bufferResource.m_pPtr);
			bufferResource.m_pPtr = GCMGL_NULL;
		}
	}
	m_BufferResources.RemoveAll();

	for (int32 i = m_ProgramResources.FirstInorder(); m_ProgramResources.IsValidIndex(i); i = m_ProgramResources.NextInorder(i))
	{
		ProgramResource_t& programResource = m_ProgramResources.Element(i);
		if (programResource.m_pVertexProgramAligned) 
		{
			rsxFree(programResource.m_pVertexProgramAligned);
		}
		if (programResource.m_pFragmentProgramAligned) 
		{
			rsxFree(programResource.m_pFragmentProgramAligned);
		}
		if (programResource.m_pFragmentProgramBuffer) 
		{
			rsxFree(programResource.m_pFragmentProgramBuffer);
		}
	}
	m_ProgramResources.RemoveAll();

	for (int32 i = m_TextureResources.FirstInorder(); m_TextureResources.IsValidIndex(i); i = m_TextureResources.NextInorder(i))
	{
		TextureResource_t& textureResource = m_TextureResources.Element(i);
		if (textureResource.m_pBuffer) rsxFree(textureResource.m_pBuffer);
	}
	m_TextureResources.RemoveAll();

	for (uint32 i = 0; i < 2; i++)
	{
		if (m_StagingVertexBuffer[i].m_pPtr)
		{
			rsxFree(m_StagingVertexBuffer[i].m_pPtr);
			m_StagingVertexBuffer[i].m_pPtr = GCMGL_NULL;
		}
		if (m_StagingIndexBuffer[i].m_pPtr)
		{
			rsxFree(m_StagingIndexBuffer[i].m_pPtr);
			m_StagingIndexBuffer[i].m_pPtr = GCMGL_NULL;
		}
	}

	ClearShaderCache();

	m_pHostAddr = GCMGL_NULL;
}

void CGcmRenderer::SetEnvironment()
{
	rsxSetColorMask(context,
		GCM_COLOR_MASK_B |
		GCM_COLOR_MASK_G |
		GCM_COLOR_MASK_R |
		GCM_COLOR_MASK_A);

	rsxSetColorMaskMrt(context, 0);

	rsxSetViewport(
		context,
		uint16(m_Viewport.m_X),
		uint16(m_Viewport.m_Y),
		uint16(m_Viewport.m_Width),
		uint16(m_Viewport.m_Height),
		m_Viewport.m_MinDepth,
		m_Viewport.m_MaxDepth,
		m_ViewportScale,
		m_ViewportOffset);
	rsxSetScissor(
		context,
		uint16(m_Viewport.m_X),
		uint16(m_Viewport.m_Y),
		uint16(m_Viewport.m_Width),
		uint16(m_Viewport.m_Height));

	rsxSetDepthTestEnable(context, GCM_TRUE);
	rsxSetDepthFunc(context, GCM_LESS);
	rsxSetDepthWriteEnable(context, 1);

	rsxSetFrontFace(context, GCM_FRONTFACE_CCW);
	rsxSetCullFaceEnable(context, GCM_TRUE);
	rsxSetCullFace(context, GCM_CULL_BACK);
}

void CGcmRenderer::BeginFrame()
{
	m_StateDirtyFlags = StateDirtyFlags_t::All;

	SetEnvironment();

	setRenderTarget(curr_fb);
}

void CGcmRenderer::EndFrame()
{
	if (context)
	{
		rsxFlushBuffer(context);
	}

	flip();
}

void CGcmRenderer::Clear(
	uint32 clearFlags,
	const CColor& color,
	float32 depth,
	uint32 stencil)
{
	uint32 clearMask = 0;
	if ((clearFlags & ClearColor) != 0)
	{
		rsxSetClearColor(context, CColor::PackARGB(color));
		clearMask |= GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A;
	}
	if ((clearFlags & ClearDepth) != 0)
	{
		clearMask |= GCM_CLEAR_Z;
	}
	if ((clearFlags & ClearStencil) != 0)
	{
		clearMask |= GCM_CLEAR_S;
	}

	uint32 depthPacked = uint32(depth * 16777215.0f);
	uint32 depthStencilClear = (depthPacked << 8) | (stencil & 0xff);

	rsxSetClearDepthStencil(context, depthStencilClear);
	rsxClearSurface(context, clearMask);
}

void CGcmRenderer::SetViewport(const Viewport_t& viewport)
{
	m_Viewport = viewport;

	rsxSetViewport(
		context,
		uint16(viewport.m_X),
		uint16(viewport.m_Y),
		uint16(viewport.m_Width),
		uint16(viewport.m_Height),
		viewport.m_MinDepth,
		viewport.m_MaxDepth,
		m_ViewportScale,
		m_ViewportOffset);
}

void CGcmRenderer::SetScissor(const Rect_t& rect)
{
	rsxSetScissor(
		context,
		uint16(rect.m_X),
		uint16(rect.m_Y),
		uint16(rect.m_Width),
		uint16(rect.m_Height));
}

void CGcmRenderer::SetStencilRef(uint32 stencilRef)
{
	rsxSetStencilFunc(context, GCM_ALWAYS, stencilRef, 0xFF);
	rsxSetStencilOp(context, GCM_KEEP, GCM_KEEP, GCM_KEEP);

	rsxSetBackStencilFunc(context, GCM_ALWAYS, stencilRef, 0xFF);
	rsxSetBackStencilOp(context, GCM_KEEP, GCM_KEEP, GCM_KEEP);
}

BufferHandle CGcmRenderer::CreateVertexBuffer(
	const void* pData,
	uint64 size,
	BufferUsage_t usage)
{
	void* pPtr = rsxMemalign(64, uint32(size));
	if (!pPtr)
	{
		Error("[GCMRenderer] Failed to allocate vertex buffer memory\n");
		
		return 0;
	}

	if (pData)
	{
		memcpy(pPtr, pData, uint32(size));
	}
	else
	{
		memset(pPtr, 0, uint32(size));
	}

	uint32 offset = 0;
	rsxAddressToOffset(pPtr, &offset);

	BufferHandle hBuffer = m_NextHandle++;
	BufferResource_t bufferResource;
	bufferResource.m_pPtr = pPtr;
	bufferResource.m_Offset = offset;
	bufferResource.m_Size = uint32(size);
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGcmRenderer::CreateIndexBuffer(
	const void* pData,
	uint64 size,
	IndexFormat_t format,
	BufferUsage_t usage)
{
	void* pPtr = rsxMemalign(64, uint32(size));
	if (!pPtr)
	{
		Error("[GCMRenderer] Failed to allocate index buffer memory\n");

		return 0;
	}

	if (pData)
	{
		memcpy(pPtr, pData, uint32(size));
	}
	else
	{
		memset(pPtr, 0, uint32(size));
	}

	uint32 offset = 0;
	rsxAddressToOffset(pPtr, &offset);

	BufferHandle hBuffer = m_NextHandle++;
	BufferResource_t bufferResource;
	bufferResource.m_pPtr = pPtr;
	bufferResource.m_Offset = offset;
	bufferResource.m_Size = uint32(size);
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGcmRenderer::CreateConstantBuffer(
	uint64 size,
	BufferUsage_t usage)
{
	void* pPtr = rsxMemalign(64, uint32(size));
	if (!pPtr)
	{
		Error("[GCMRenderer] Failed to allocate constant buffer memory\n");

		return 0;
	}

	uint32 offset = 0;
	rsxAddressToOffset(pPtr, &offset);

	BufferHandle hBuffer = m_NextHandle++;
	BufferResource_t bufferResource;
	bufferResource.m_pPtr = pPtr;
	bufferResource.m_Offset = offset;
	bufferResource.m_Size = uint32(size);
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

void CGcmRenderer::UpdateBuffer(
	BufferHandle hBuffer,
	const void* pData,
	uint64 size,
	uint64 offset)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	BufferResource_t& bufferResource = m_BufferResources.Element(bufferIndex);
	uint32 updateSize = (size == 0) ? bufferResource.m_Size : uint32(size);
	memcpy(
		reinterpret_cast<uint8*>(bufferResource.m_pPtr) + offset,
		pData,
		updateSize);
}

void CGcmRenderer::DestroyBuffer(BufferHandle hBuffer)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	void* pPtr = m_BufferResources.Element(bufferIndex).m_pPtr;
	if (pPtr) rsxFree(pPtr);

	m_BufferResources.Remove(hBuffer);
}

void* CGcmRenderer::MapBuffer(BufferHandle hBuffer)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid buffer handle: %d\n", hBuffer);

		return GCMGL_NULL;
	}

	return m_BufferResources.Element(bufferIndex).m_pPtr;
}

void CGcmRenderer::UnmapBuffer(BufferHandle hBuffer)
{
	Warning("[GCMRenderer] UnmapBuffer not implemented\n");
}

ShaderProgramHandle CGcmRenderer::CreateShaderProgram(const CFixedString& shaderName)
{
	CFixedString vertexProgramPath = CFixedString("shaders/cg/") + shaderName + CFixedString(".vpo");
	CUtlVector<uint8> vertexProgramBin = CUtils::ReadBinaryFile(
		vertexProgramPath);
	if (vertexProgramBin.Count() == 0)
	{
		Warning(
			"[GCMRenderer] Failed to read vertex program binary: %s\n",
			vertexProgramPath.Get());

		return 0;
	}

	void* pVertexProgramAligned = rsxMemalign(
		64,
		uint32(vertexProgramBin.Count()));
	memcpy(
		pVertexProgramAligned,
		vertexProgramBin.Base(),
		uint32(vertexProgramBin.Count()));

	void* pVertexProgramUCode = GCMGL_NULL;
	uint32 vertexProgramSize = 0;
	const rsxVertexProgram* pVertexProgram = reinterpret_cast<const rsxVertexProgram*>(
		pVertexProgramAligned);
	rsxVertexProgramGetUCode(
		const_cast<rsxVertexProgram*>(pVertexProgram),
		&pVertexProgramUCode,
		&vertexProgramSize);

	CFixedString fragmentProgramPath = CFixedString("shaders/cg/") + shaderName + CFixedString(".fpo");
	CUtlVector<uint8> fragmentProgramBin = CUtils::ReadBinaryFile(
		fragmentProgramPath);
	if (fragmentProgramBin.Count() == 0)
	{
		Warning(
			"[GCMRenderer] Failed to read fragment program binary: %s\n",
			fragmentProgramPath.Get());
		
		rsxFree(pVertexProgramAligned);

		return 0;
	}

	void* pFragmentProgramAligned = rsxMemalign(
		64,
		uint32(fragmentProgramBin.Count()));
	memcpy(
		pFragmentProgramAligned,
		fragmentProgramBin.Base(),
		uint32(fragmentProgramBin.Count()));

	void* pFragmentProgramCode = GCMGL_NULL;
	uint32 fragmentProgramSize = 0;
	const rsxFragmentProgram* pFragmentProgram = reinterpret_cast<const rsxFragmentProgram*>(
		pFragmentProgramAligned);
	rsxFragmentProgramGetUCode(
		const_cast<rsxFragmentProgram*>(pFragmentProgram),
		&pFragmentProgramCode,
		&fragmentProgramSize);

	uint32 fragmentProgramOffset = 0;
	void* pFragmentProgramBuffer = rsxMemalign(64, fragmentProgramSize);
	memcpy(pFragmentProgramBuffer, pFragmentProgramCode, fragmentProgramSize);
	rsxAddressToOffset(pFragmentProgramBuffer, &fragmentProgramOffset);

	ShaderProgramHandle hProgram = m_NextHandle++;
	ProgramResource_t programResource;
	programResource.m_pVertexProgramAligned = pVertexProgramAligned;
	programResource.m_pVertexProgram = pVertexProgram;
	programResource.m_pVertexProgramUCode = pVertexProgramUCode;
	programResource.m_VertexProgramSize = vertexProgramSize;
	programResource.m_pFragmentProgramAligned = pFragmentProgramAligned;
	programResource.m_pFragmentProgram = pFragmentProgram;
	programResource.m_pFragmentProgramBuffer = pFragmentProgramBuffer;
	programResource.m_FragmentProgramOffset = fragmentProgramOffset;
	programResource.m_FragmentProgramSize = fragmentProgramSize;
	m_ProgramResources.Insert(hProgram, programResource);

	return hProgram;
}

void CGcmRenderer::DestroyShaderProgram(ShaderProgramHandle hProgram)
{
	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);
	if (programResource.m_pVertexProgramAligned) rsxFree(
		programResource.m_pVertexProgramAligned);
	if (programResource.m_pFragmentProgramAligned) rsxFree(
		programResource.m_pFragmentProgramAligned);
	if (programResource.m_pFragmentProgramBuffer) rsxFree(
		programResource.m_pFragmentProgramBuffer);

	m_ProgramResources.Remove(hProgram);
}

TextureHandle CGcmRenderer::CreateTexture2D(
	uint32 width,
	uint32 height,
	TextureFormat_t format,
	const void* pData)
{
	// 4 bytes per pixel (ARGB8)
	uint32 textureBufferSize = width * height * 4;
	void* pTextureBuffer = rsxMemalign(128, textureBufferSize);
	if (!pTextureBuffer)
	{
		Warning("[GCMRenderer] Failed to allocate texture buffer memory\n");

		return 0;
	}

	if (pData)
	{
		uint8* pDst = reinterpret_cast<uint8*>(pTextureBuffer);
		const uint8* pSrc = reinterpret_cast<const uint8*>(pData);

		switch (format)
		{
			case TextureFormat_t::R8:
				for (uint32 i = 0, j = 0; i < width * height; i++, j += 4)
				{
					pDst[j + 0] = 255;
					pDst[j + 1] = 0;
					pDst[j + 2] = 0;
					pDst[j + 3] = pSrc[i];
				}

				break;
			case TextureFormat_t::RG8:
				for (uint32 i = 0, j = 0; i < width * height * 2; i += 2, j += 4)
				{
					pDst[j + 0] = 255;
					pDst[j + 1] = 0;
					pDst[j + 2] = pSrc[i];
					pDst[j + 3] = pSrc[i + 1];
				}

				break;
			case TextureFormat_t::RGB8:
				for (uint32 i = 0, j = 0; i < width * height * 3; i += 3, j += 4)
				{
					pDst[j + 0] = 255;
					pDst[j + 1] = pSrc[i];
					pDst[j + 2] = pSrc[i + 1];
					pDst[j + 3] = pSrc[i + 2];
				}

				break;
			case TextureFormat_t::RGBA8:
				for (uint32 i = 0; i < width * height * 4; i += 4)
				{
					pDst[i + 0] = pSrc[i + 3];
					pDst[i + 1] = pSrc[i + 0];
					pDst[i + 2] = pSrc[i + 1];
					pDst[i + 3] = pSrc[i + 2];
				}

				break;
			default:
				memcpy(pDst, pSrc, textureBufferSize);

				break;
		}
	}

	uint32 textureOffset;
	rsxAddressToOffset(pTextureBuffer, &textureOffset);

	TextureResource_t textureResource;
	textureResource.m_pBuffer = pTextureBuffer;
	textureResource.m_Offset = textureOffset;
	textureResource.m_Width = width;
	textureResource.m_Height = height;
	textureResource.m_Format = format;
	textureResource.m_IsCubemap = false;

	TextureHandle hTexture = m_NextHandle++;
	m_TextureResources.Insert(hTexture, textureResource);

	return hTexture;
}

TextureHandle CGcmRenderer::CreateTextureCube(
	uint32 size,
	TextureFormat_t format,
	const void** pFaces)
{
	// 6 faces, 4 bytes per pixel (ARGB8)
	uint32 textureBufferSize = size * size * 4 * 6;
	void* pTextureBuffer = rsxMemalign(128, textureBufferSize);
	if (!pTextureBuffer)
	{
		Warning("[GCMRenderer] Failed to allocate texture buffer memory\n");

		return 0;
	}

	for (int32 i = 0; i < 6; i++)
	{
		if (!pFaces || !pFaces[i]) continue;

		uint8* pDst = reinterpret_cast<uint8*>(pTextureBuffer) + uint32(i) * size * size * 4;
		const uint8* pSrc = reinterpret_cast<const uint8*>(pFaces[i]);

		switch (format)
		{
			case TextureFormat_t::R8:
				for (uint32 j = 0, k = 0; j < size * size; j++, k += 4)
				{
					pDst[k + 0] = 255;
					pDst[k + 1] = 0;
					pDst[k + 2] = 0;
					pDst[k + 3] = pSrc[j];
				}
				
				break;
			case TextureFormat_t::RG8:
				for (uint32 j = 0, k = 0; j < size * size * 2; j += 2, k += 4)
				{
					pDst[k + 0] = 255;
					pDst[k + 1] = 0;
					pDst[k + 2] = pSrc[j];
					pDst[k + 3] = pSrc[j + 1];
				}

				break;
			case TextureFormat_t::RGB8:
				for (uint32 j = 0, k = 0; j < size * size * 3; j += 3, k += 4)
				{
					pDst[k + 0] = 255;
					pDst[k + 1] = pSrc[j];
					pDst[k + 2] = pSrc[j + 1];
					pDst[k + 3] = pSrc[j + 2];
				}

				break;
			case TextureFormat_t::RGBA8:
				for (uint32 j = 0; j < size * size * 4; j += 4)
				{
					pDst[j + 0] = pSrc[j + 3];
					pDst[j + 1] = pSrc[j + 0];
					pDst[j + 2] = pSrc[j + 1];
					pDst[j + 3] = pSrc[j + 2];
				}

				break;
			default:
				memcpy(pDst, pSrc, size * size * 4);

				break;
		}
	}

	uint32 textureOffset;
	rsxAddressToOffset(pTextureBuffer, &textureOffset);

	TextureResource_t textureResource;
	textureResource.m_pBuffer = pTextureBuffer;
	textureResource.m_Offset = textureOffset;
	textureResource.m_Width = size;
	textureResource.m_Height = size;
	textureResource.m_Format = format;
	textureResource.m_IsCubemap = true;

	TextureHandle hTexture = m_NextHandle++;
	m_TextureResources.Insert(hTexture, textureResource);

	return hTexture;
}

void CGcmRenderer::SetTexture(
	TextureHandle hTexture,
	uint32 slot,
	ShaderStage_t stage)
{
	if (stage != ShaderStageFragment)
	{
		return;
	}

	int32 textureIndex = m_TextureResources.Find(hTexture);
	if (textureIndex == m_TextureResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid texture handle: %d\n", hTexture);

		return;
	}

	const TextureResource_t& textureResource = m_TextureResources.Element(
		textureIndex);

	uint32 gcmTextureFormat = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
	uint32 texturePitch = textureResource.m_Width * 4;

	switch (textureResource.m_Format)
	{
		case TextureFormat_t::R8:
			gcmTextureFormat = GCM_TEXTURE_FORMAT_B8 | GCM_TEXTURE_FORMAT_LIN;
			texturePitch = textureResource.m_Width * 1;

			break;
		case TextureFormat_t::RG8:
			gcmTextureFormat = GCM_TEXTURE_FORMAT_G8B8 | GCM_TEXTURE_FORMAT_LIN;
			texturePitch = textureResource.m_Width * 2;

			break;
		case TextureFormat_t::RGB8:
		case TextureFormat_t::RGBA8:
		default:
			gcmTextureFormat = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
			texturePitch = textureResource.m_Width * 4;

			break;
	}

	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);

	gcmTexture gcmTex;
	gcmTex.format = gcmTextureFormat;
	gcmTex.mipmap = 1;
	gcmTex.dimension = GCM_TEXTURE_DIMS_2D;
	gcmTex.cubemap = textureResource.m_IsCubemap ? GCM_TRUE : GCM_FALSE;
	gcmTex.remap = 
		((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
		(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
		(GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
	gcmTex.width = textureResource.m_Width;
	gcmTex.height = textureResource.m_Height;
	gcmTex.depth = textureResource.m_IsCubemap ? 6 : 1;
	gcmTex.location = GCM_LOCATION_RSX;
	gcmTex.pitch = texturePitch;
	gcmTex.offset = textureResource.m_Offset;
	rsxLoadTexture(context, slot, &gcmTex);

	rsxTextureControl(
		context,
		slot,
		GCM_TRUE,
		0 << 8,
		12 << 8,
		GCM_TEXTURE_MAX_ANISO_1);
	rsxTextureFilter(
		context,
		slot,
		0,
		GCM_TEXTURE_LINEAR,
		GCM_TEXTURE_LINEAR,
		GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	rsxTextureWrapMode(
		context,
		slot,
		GCM_TEXTURE_CLAMP_TO_EDGE,
		GCM_TEXTURE_CLAMP_TO_EDGE,
		GCM_TEXTURE_CLAMP_TO_EDGE,
		0,
		GCM_TEXTURE_ZFUNC_LESS,
		0);
}

void CGcmRenderer::SetSampler(
	SamplerHandle hSampler,
	uint32 slot,
	ShaderStage_t stage)
{
	Warning("[GCMRenderer] SetSampler not implemented\n");
}

void CGcmRenderer::UpdateTexture(
	TextureHandle hTexture,
	const void* pData,
	uint32 mipLevel)
{
	Warning("[GCMRenderer] UpdateTexture not implemented\n");
}

void CGcmRenderer::DestroyTexture(TextureHandle hTexture)
{
	int32 textureIndex = m_TextureResources.Find(hTexture);
	if (textureIndex == m_TextureResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid texture handle: %d\n", hTexture);

		return;
	}

	void* pTextureBuffer = m_TextureResources.Element(textureIndex).m_pBuffer;
	if (pTextureBuffer) rsxFree(pTextureBuffer);

	m_TextureResources.Remove(hTexture);
}

void CGcmRenderer::SetConstantBuffer(
	BufferHandle hBuffer,
	UniformBlockLayoutHandle hLayout,
	uint32 slot,
	ShaderStage_t stage)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	int32 layoutIndex = m_UniformBlockLayouts.Find(hLayout);
	if (layoutIndex == m_UniformBlockLayouts.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid uniform block layout handle: %d\n",
			hLayout);

		return;
	}

	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	BoundUniform_t boundUniform;
	boundUniform.m_hBuffer = hBuffer;
	boundUniform.m_Slot = slot;
	boundUniform.m_Stage = stage;
	m_ProgramUniformBuffers[m_PipelineState.m_hShaderProgram].Insert(
		hLayout,
		boundUniform);

	UniformShadow_t& uniformShadow = m_ProgramUniformShadows[m_PipelineState.m_hShaderProgram][hLayout];
	const UniformBlockLayout_t& uniformLayout = m_UniformBlockLayouts.Element(
		layoutIndex);
	if (uniformShadow.m_Data.Count() < int32(uniformLayout.m_Size))
	{
		uniformShadow.m_Data.SetCount(int32(uniformLayout.m_Size));
	}
	memcpy(
		uniformShadow.m_Data.Base(),
		m_BufferResources.Element(bufferIndex).m_pPtr,
		uniformLayout.m_Size);
	uniformShadow.m_IsDirty = true;

	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::Uniforms;
}

void CGcmRenderer::SetBlendState(const BlendState_t& state)
{
	if (state.m_IsEnabled)
	{
		rsxSetBlendEnable(context, GCM_TRUE);
		rsxSetBlendFunc(
			context,
			GCM_SRC_ALPHA,
			GCM_ONE_MINUS_SRC_ALPHA,
			GCM_ONE,
			GCM_ZERO);
		rsxSetBlendEquation(context, GCM_FUNC_ADD, GCM_FUNC_ADD);
	}
	else
	{
		rsxSetBlendEnable(context, GCM_FALSE);
	}
}

void CGcmRenderer::SetDepthStencilState(const DepthStencilState_t& state)
{
	rsxSetDepthTestEnable(context, state.m_IsDepthTest ? GCM_TRUE : GCM_FALSE);
	rsxSetDepthFunc(context, GCM_LESS);
	rsxSetDepthWriteEnable(
		context,
		state.m_IsDepthWrite ? GCM_TRUE : GCM_FALSE);
}

void CGcmRenderer::ApplyVertexConstants(ShaderProgramHandle hProgram)
{
	if (hProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	int32 uniformsIndex = m_ProgramUniformBuffers.Find(hProgram);
	if (uniformsIndex == m_ProgramUniformBuffers.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] No uniform buffers bound for program: %d\n", 
			hProgram);

		return;
	}

	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	CUtlMap<uint32, BoundUniform_t>& uniforms = m_ProgramUniformBuffers.Element(
		uniformsIndex);
	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);

	for (int32 uniformIndex = uniforms.FirstInorder(); uniforms.IsValidIndex(uniformIndex); uniformIndex = uniforms.NextInorder(uniformIndex))
	{
		const BoundUniform_t& boundUniform = uniforms.Element(uniformIndex);
		if (!(boundUniform.m_Stage & ShaderStageVertex)) continue;

		uint32 hLayout = uniforms.Key(uniformIndex);
		int32 layoutIndex = m_UniformBlockLayouts.Find(hLayout);
		if (layoutIndex == m_UniformBlockLayouts.InvalidIndex()) continue;

		int32 bufferIndex = m_BufferResources.Find(boundUniform.m_hBuffer);
		if (bufferIndex == m_BufferResources.InvalidIndex()) continue;

		UniformShadow_t& uniformShadow = m_ProgramUniformShadows[hProgram][hLayout];
		const UniformBlockLayout_t& uniformLayout = m_UniformBlockLayouts.Element(
			layoutIndex);
		if (uniformShadow.m_Data.Count() < int32(uniformLayout.m_Size))
		{
			uniformShadow.m_Data.SetCount(int32(uniformLayout.m_Size));
			uniformShadow.m_IsDirty = true;
		}

		const float32* pBufferData = reinterpret_cast<const float32*>(
			m_BufferResources.Element(bufferIndex).m_pPtr);
		
		bool hasChanged = uniformShadow.m_IsDirty || (memcmp(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size) != 0);
		if (!hasChanged) continue;

		memcpy(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size);
		__sync_synchronize();

		if (uniformLayout.m_UniformNames.IsEmpty()) continue;

		CUtlMap<CFixedString, rsxProgramConst*>& constCache = programResource.m_VertexProgramConstCache;
		const uint8* pShadowData = uniformShadow.m_Data.Base();

		uint32 uniformBytes = uniformLayout.m_Size / uniformLayout.m_UniformNames.Count();

		for (int32 uniformNameIndex = 0; uniformNameIndex < uniformLayout.m_UniformNames.Count(); uniformNameIndex++)
		{
			const CFixedString& uniformName = uniformLayout.m_UniformNames[uniformNameIndex];

			rsxProgramConst* pConst = GCMGL_NULL;
			int32 constCacheIndex = constCache.Find(uniformName);
			if (constCacheIndex != constCache.InvalidIndex())
			{
				pConst = constCache.Element(constCacheIndex);
			}
			else
			{
				pConst = rsxVertexProgramGetConst(
					const_cast<rsxVertexProgram*>(
						programResource.m_pVertexProgram),
					uniformName.Get());
				constCache.Insert(uniformName, pConst);
			}

			if (!pConst) continue;

			const float32* pElementData = reinterpret_cast<const float32*>(
				pShadowData + uint32(uniformNameIndex) * uniformBytes);

			if (uniformBytes == sizeof(CMatrix4))
			{
				float32 transposed[16];
				for (uint32 row = 0; row < 4; row++)
				{
					for (uint32 col = 0; col < 4; col++)
					{
						transposed[row * 4 + col] = pElementData[col * 4 + row];
					}
				}

				rsxSetVertexProgramParameter(
					context,
					const_cast<rsxVertexProgram*>(
						programResource.m_pVertexProgram),
					pConst,
					transposed);
			}
			else
			{
				rsxSetVertexProgramParameter(
					context,
					const_cast<rsxVertexProgram*>(
						programResource.m_pVertexProgram),
					pConst,
					pElementData);
			}
		}

		uniformShadow.m_IsDirty = false;
	}
}

void CGcmRenderer::ApplyFragmentConstants(ShaderProgramHandle hProgram)
{
	if (hProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	int32 uniformsIndex = m_ProgramUniformBuffers.Find(hProgram);
	if (uniformsIndex == m_ProgramUniformBuffers.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] No uniform buffers bound for program: %d\n", 
			hProgram);

		return;
	}

	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning("[GCMRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	CUtlMap<uint32, BoundUniform_t>& uniforms = m_ProgramUniformBuffers.Element(
		uniformsIndex);
	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);
	
	for (int32 uniformIndex = uniforms.FirstInorder(); uniforms.IsValidIndex(uniformIndex); uniformIndex = uniforms.NextInorder(uniformIndex))
	{
		const BoundUniform_t& boundUniform = uniforms.Element(uniformIndex);
		if (!(boundUniform.m_Stage & ShaderStageFragment)) continue;

		uint32 hLayout = uniforms.Key(uniformIndex);
		int32 layoutIndex = m_UniformBlockLayouts.Find(hLayout);
		if (layoutIndex == m_UniformBlockLayouts.InvalidIndex()) continue;

		int32 bufferIndex = m_BufferResources.Find(boundUniform.m_hBuffer);
		if (bufferIndex == m_BufferResources.InvalidIndex()) continue;

		UniformShadow_t& uniformShadow = m_ProgramUniformShadows[hProgram][hLayout];
		const UniformBlockLayout_t& uniformLayout = m_UniformBlockLayouts.Element(
			layoutIndex);
		if (uniformShadow.m_Data.Count() < int32(uniformLayout.m_Size))
		{
			uniformShadow.m_Data.SetCount(int32(uniformLayout.m_Size));
			uniformShadow.m_IsDirty = true;
		}

		const float32* pBufferData = reinterpret_cast<const float32*>(
			m_BufferResources.Element(bufferIndex).m_pPtr);
		
		bool hasChanged = uniformShadow.m_IsDirty || (memcmp(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size) != 0);
		if (!hasChanged) continue;

		memcpy(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size);
		__sync_synchronize();

		if (uniformLayout.m_UniformNames.IsEmpty()) continue;

		rsxProgramConst* pFragmentConsts = rsxFragmentProgramGetConsts(
			const_cast<rsxFragmentProgram*>(programResource.m_pFragmentProgram));
		
		const uint8* pShadowData = uniformShadow.m_Data.Base();
		uint32 uniformBytes = uniformLayout.m_Size / uniformLayout.m_UniformNames.Count();

		for (int32 uniformNameIndex = 0; uniformNameIndex < uniformLayout.m_UniformNames.Count(); uniformNameIndex++)
		{
			if (!pFragmentConsts) continue;

			rsxProgramConst* pConst = rsxFragmentProgramGetConst(
				const_cast<rsxFragmentProgram*>(
					programResource.m_pFragmentProgram),
				uniformLayout.m_UniformNames[uniformNameIndex].Get());
			if (!pConst) continue;

			int32 index = int32(pConst - pFragmentConsts);

			rsxSetFragmentProgramParameter(
				context,
				const_cast<rsxFragmentProgram*>(
					programResource.m_pFragmentProgram),
				&pFragmentConsts[index],
				reinterpret_cast<const float32*>(pShadowData + uint32(uniformNameIndex) * uniformBytes),
				programResource.m_FragmentProgramOffset,
				GCM_LOCATION_RSX);
		}

		rsxLoadFragmentProgramLocation(
			context,
			const_cast<rsxFragmentProgram*>(programResource.m_pFragmentProgram),
			programResource.m_FragmentProgramOffset,
			GCM_LOCATION_RSX);

		uniformShadow.m_IsDirty = false;
	}
}

void CGcmRenderer::Draw(
	uint32 vertexCount,
	uint32 startVertex,
	const CMatrix4* pViewProj,
	const CVector3* pAABBCenter,
	const CVector3* pAABBExtent)
{
	if (pViewProj && pAABBCenter && pAABBExtent)
	{
		Plane_t frustumPlanes[6];
		ExtractFrustumPlanes(*pViewProj, frustumPlanes);
		if (!TestAABBFrustum(*pAABBCenter, *pAABBExtent, frustumPlanes))
		{
			return;
		}
	}

	rsxInvalidateVertexCache(context);
	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);

	FlushPipelineState();

	rsxDrawVertexArray(context, GCM_TYPE_TRIANGLES, startVertex, vertexCount);
}

void CGcmRenderer::DrawIndexed(
	uint32 indexCount,
	uint32 startIndex,
	int32 baseVertex,
	const CMatrix4* pViewProj,
	const CVector3* pAABBCenter,
	const CVector3* pAABBExtent)
{
	if (m_PipelineState.m_hIndexBuffer == 0)
	{
		Warning("[GCMRenderer] Invalid index buffer bound\n");

		return;
	}

	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	if (pViewProj && pAABBCenter && pAABBExtent)
	{
		Plane_t frustumPlanes[6];
		ExtractFrustumPlanes(*pViewProj, frustumPlanes);
		if (!TestAABBFrustum(*pAABBCenter, *pAABBExtent, frustumPlanes))
		{
			return;
		}
	}

	int32 indexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hIndexBuffer);
	if (indexBufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid index buffer handle: %d\n",
			m_PipelineState.m_hIndexBuffer);

		return;
	}

	rsxInvalidateVertexCache(context);
	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);

	FlushPipelineState();

	uint32 indexOffset = m_BufferResources.Element(indexBufferIndex).m_Offset + (startIndex * uint32(sizeof(uint32))) + uint32(m_PipelineState.m_IndexOffset);
	rsxDrawIndexArray(
		context,
		GCM_TYPE_TRIANGLES,
		indexOffset,
		indexCount,
		GCM_INDEX_TYPE_32B,
		GCM_LOCATION_RSX);
}

uint32 CGcmRenderer::GetSemanticAttributeIndex(VertexSemantic_t semantic)
{
	switch (semantic)
	{
		case VertexSemantic_t::Position: return GCM_VERTEX_ATTRIB_POS;
		case VertexSemantic_t::Weight: return GCM_VERTEX_ATTRIB_WEIGHT;
		case VertexSemantic_t::Normal: return GCM_VERTEX_ATTRIB_NORMAL;
		case VertexSemantic_t::Color0: return GCM_VERTEX_ATTRIB_COLOR0;
		case VertexSemantic_t::Color1: return GCM_VERTEX_ATTRIB_COLOR1;
		case VertexSemantic_t::Fog: return GCM_VERTEX_ATTRIB_FOG;
		case VertexSemantic_t::PointSize: return GCM_VERTEX_ATTRIB_POINT_SIZE;
		case VertexSemantic_t::EdgeFlag: return GCM_VERTEX_ATTRIB_EDGEFLAG;
		case VertexSemantic_t::TexCoord0: return GCM_VERTEX_ATTRIB_TEX0;
		case VertexSemantic_t::TexCoord1: return GCM_VERTEX_ATTRIB_TEX1;
		case VertexSemantic_t::TexCoord2: return GCM_VERTEX_ATTRIB_TEX2;
		case VertexSemantic_t::TexCoord3: return GCM_VERTEX_ATTRIB_TEX3;
		case VertexSemantic_t::TexCoord4: return GCM_VERTEX_ATTRIB_TEX4;
		case VertexSemantic_t::TexCoord5: return GCM_VERTEX_ATTRIB_TEX5;
		case VertexSemantic_t::TexCoord6: return GCM_VERTEX_ATTRIB_TEX6;
		case VertexSemantic_t::Tangent: return GCM_VERTEX_ATTRIB_TANGENT;
		case VertexSemantic_t::TexCoord7: return GCM_VERTEX_ATTRIB_TEX7;
		case VertexSemantic_t::Binormal: return GCM_VERTEX_ATTRIB_BINORMAL;
		default: return 0xFFFFFFFF;
	}
}

void CGcmRenderer::BindVertexAttributes(
	const CVertexLayout* pLayout,
	uint32 stride,
	uint32 offset)
{
	if (!pLayout)
	{
		Warning("[GCMRenderer] Invalid vertex layout\n");

		return;
	}

	if (m_PipelineState.m_hVertexBuffer == 0)
	{
		Warning("[GCMRenderer] Invalid vertex buffer bound\n");

		return;
	}

	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid vertex buffer handle: %d\n",
			m_PipelineState.m_hVertexBuffer);

		return;
	}

	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	if (m_ProgramResources.Find(m_PipelineState.m_hShaderProgram) == m_ProgramResources.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid shader program handle: %d\n",
			m_PipelineState.m_hShaderProgram);

		return;
	}

	uint32 vertexStride = stride ? stride : pLayout->GetStride();

	const CUtlVector<VertexAttribute_t>& attributes = pLayout->GetAttributes();
	for (int32 attributeIndex = 0; attributeIndex < attributes.Count(); attributeIndex++)
	{
		const VertexAttribute_t& attribute = attributes[attributeIndex];
		if (attribute.m_Name.IsEmpty()) continue;

		uint32 attributeLocation = 0xFFFFFFFF;
		if (attribute.m_Semantic != VertexSemantic_t::Unspecified)
		{
			attributeLocation = GetSemanticAttributeIndex(
				attribute.m_Semantic);
		}

		if (attributeLocation == 0xFFFFFFFF) continue;

		uint32 components = 3;
		uint8 dataType = GCM_VERTEX_DATA_TYPE_F32;

		switch (attribute.m_Format)
		{
			case VertexFormat_t::Float:
				components = 1;
				dataType = GCM_VERTEX_DATA_TYPE_F32;

				break;
			case VertexFormat_t::Float2:
				components = 2;
				dataType = GCM_VERTEX_DATA_TYPE_F32;

				break;
			case VertexFormat_t::Float3:
				components = 3;
				dataType = GCM_VERTEX_DATA_TYPE_F32;

				break;
			case VertexFormat_t::Float4:
				components = 4;
				dataType = GCM_VERTEX_DATA_TYPE_F32;

				break;
			case VertexFormat_t::UByte4_Norm:
				components = 4;
				dataType = GCM_VERTEX_DATA_TYPE_U8;

				break;
			default:
				components = 3;
				dataType = GCM_VERTEX_DATA_TYPE_F32;

				break;
		}

		void* pAttributePtr = reinterpret_cast<char*>(
			m_BufferResources.Element(vertexBufferIndex).m_pPtr) + offset + attribute.m_Offset;
		uint32 attributeOffset = 0;
		rsxAddressToOffset(pAttributePtr, &attributeOffset);

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			0,
			attributeOffset,
			uint16(vertexStride),
			uint8(components),
			dataType,
			GCM_LOCATION_RSX);
	}

	rsxInvalidateVertexCache(context);
}

void CGcmRenderer::FlushProgramState()
{
	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GCMRenderer] Invalid shader program bound\n");

		return;
	}

	int32 programIndex = m_ProgramResources.Find(
		m_PipelineState.m_hShaderProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid shader program handle: %d\n",
			m_PipelineState.m_hShaderProgram);
		
		return;
	}

	const ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);
	rsxLoadVertexProgram(
		context,
		const_cast<rsxVertexProgram*>(programResource.m_pVertexProgram),
		programResource.m_pVertexProgramUCode);
	rsxLoadFragmentProgramLocation(
		context,
		const_cast<rsxFragmentProgram*>(programResource.m_pFragmentProgram),
		programResource.m_FragmentProgramOffset,
		GCM_LOCATION_RSX);
}

IRenderer* CreateRenderer()
{
	void* pRendererMemory = CUtlMemory::Alloc(sizeof(CGcmBatchRenderer));
	if (pRendererMemory)
	{
		return new(pRendererMemory) CGcmBatchRenderer();
	}

	return GCMGL_NULL;
}

void DestroyRenderer(IRenderer* pRenderer)
{
	if (pRenderer)
	{
		pRenderer->~IRenderer();
		CUtlMemory::Free(pRenderer);
	}
}