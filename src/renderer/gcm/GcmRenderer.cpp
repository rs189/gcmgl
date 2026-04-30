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
#include <unistd.h>
#include <sysutil/sysutil.h>

#ifdef SIMD_ENABLED
#include <altivec.h>
#undef pixel
#undef bool
#endif

#ifdef PS3_SPU_ENABLED
#include "platform/ps3/spu/SpuCommon.h"
#include "spu/SpuBatchTransformManager.h"
#endif // PS3_SPU_ENABLED

CGcmRenderer::CGcmRenderer() :
#ifdef PS3_SPU_ENABLED
	m_pSpuBatchTransformManager(GCMGL_NULL),
#endif // PS3_SPU_ENABLED
	m_pHostAddr(GCMGL_NULL),
	m_HostSize(0),
	m_StagingIndex(0),
	m_InstanceBufferIndex(0)
{
	memset(&m_PostProcessState, 0, sizeof(m_PostProcessState));
}

CGcmRenderer::~CGcmRenderer()
{
	Shutdown();
}

bool CGcmRenderer::Init(const RendererDesc_t& rendererDesc)
{
	m_HostSize = 1024 * 1024 * 128; // 128MB
	sys_mem_addr_t hostAddr;
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
	void* pSpuBatchTransformManagerMemory = CUtlMemory::Alloc(
		sizeof(CSpuBatchTransformManager));
	if (pSpuBatchTransformManagerMemory)
	{
		m_pSpuBatchTransformManager = new(pSpuBatchTransformManagerMemory) CSpuBatchTransformManager();
		if (!m_pSpuBatchTransformManager->Init())
		{
			m_pSpuBatchTransformManager->~CSpuBatchTransformManager();
			CUtlMemory::Free(pSpuBatchTransformManagerMemory);
			m_pSpuBatchTransformManager = GCMGL_NULL;
		}
	}

#endif // PS3_SPU_ENABLED

	if (!m_StaticHeap.Init(s_StaticHeapSize))
	{
		Error("[GCMRenderer] Failed to allocate static heap\n");

		return false;
	}

	if (!m_DynamicHeap.Init(s_DynamicHeapSize))
	{
		Error("[GCMRenderer] Failed to allocate dynamic heap\n");

		return false;
	}

	m_PostProcessState = CGcmPostProcessingRenderer::InitState(m_StaticHeap);

	return true;
}

void CGcmRenderer::Shutdown()
{
	if (!m_pHostAddr) return;

#ifdef PS3_SPU_ENABLED
	if (m_pSpuBatchTransformManager)
	{
		m_pSpuBatchTransformManager->Shutdown();
		m_pSpuBatchTransformManager->~CSpuBatchTransformManager();
		CUtlMemory::Free(m_pSpuBatchTransformManager);
		m_pSpuBatchTransformManager = GCMGL_NULL;
	}

#endif // PS3_SPU_ENABLED

	rsxFlushBuffer(context);
	waitFinish();

	CGcmPostProcessingRenderer::ShutdownState(m_PostProcessState, m_StaticHeap);

	for (uint32 i = 0; i < 2; i++)
	{
		if (m_StagingVertexBuffer[i].m_pPtr)
		{
			m_DynamicHeap.Free(m_StagingVertexBuffer[i].m_Alloc);
			m_StagingVertexBuffer[i].m_pPtr = GCMGL_NULL;
		}
		if (m_StagingIndexBuffer[i].m_pPtr)
		{
			m_DynamicHeap.Free(m_StagingIndexBuffer[i].m_Alloc);
			m_StagingIndexBuffer[i].m_pPtr = GCMGL_NULL;
		}
	}
	for (int32 i = 0; i < s_MaxInstanceStagingBuffers; i++)
	{
		if (m_InstanceBuffers[i].m_pPtr)
		{
			m_DynamicHeap.Free(m_InstanceBuffers[i].m_Alloc);
			m_InstanceBuffers[i].m_pPtr = GCMGL_NULL;
		}
	}

	for (int32 i = m_TextureResources.FirstInorder(); m_TextureResources.IsValidIndex(i); i = m_TextureResources.NextInorder(i))
	{
		TextureResource_t& textureResource = m_TextureResources.Element(i);
		if (textureResource.m_pBuffer)
		{
			m_StaticHeap.Free(textureResource.m_Alloc);
		}
	}
	m_TextureResources.RemoveAll();

	for (int32 i = m_ProgramResources.FirstInorder(); m_ProgramResources.IsValidIndex(i); i = m_ProgramResources.NextInorder(i))
	{
		ProgramResource_t& programResource = m_ProgramResources.Element(i);
		if (programResource.m_pFragmentProgramAligned)
		{
			m_StaticHeap.Free(programResource.m_FragmentProgramAlloc);
		}
		if (programResource.m_pFragmentProgramBuffer)
		{
			m_StaticHeap.Free(programResource.m_FragmentProgramBufferAlloc);
		}
		if (programResource.m_pVertexProgramAligned)
		{
			CUtlMemory::Free(programResource.m_pVertexProgramAligned);
		}
	}
	m_ProgramResources.RemoveAll();

	m_ProgramUniformShadows.RemoveAll();
	m_ProgramUniformBuffers.RemoveAll();

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
			m_StaticHeap.Free(bufferResource.m_Alloc);
			bufferResource.m_pPtr = GCMGL_NULL;
		}
	}
	m_BufferResources.RemoveAll();

	ClearShaderCache();

	m_DynamicHeap.Shutdown();
	m_StaticHeap.Shutdown();

	m_pHostAddr = GCMGL_NULL;
	m_HostSize = 0;
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
	waitFlip();

	m_StateDirtyFlags = StateDirtyFlags_t::All;
	m_InstanceBufferIndex = 0;

	SetEnvironment();

	if (m_PostProcessState.m_pVertexProgram &&
		m_PostProcessState.m_pQuadVertices &&
		m_PostProcessState.m_pFragmentProgramBuffer)
	{
		CGcmPostProcessingRenderer::Begin(m_PostProcessState);
	}
	else
	{
		setRenderTarget(curr_fb);
	}
}

void CGcmRenderer::EndFrame()
{
	if (context)
	{
		rsxFlushBuffer(context);
		waitFinish();
	}

	if (m_PostProcessState.m_pVertexProgram &&
		m_PostProcessState.m_pQuadVertices &&
		m_PostProcessState.m_pFragmentProgramBuffer)
	{
		CGcmPostProcessingRenderer::End(m_PostProcessState);
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

void CGcmRenderer::GetFramebufferSize(uint32& width, uint32& height) const
{
	width = display_width;
	height = display_height;
}

void CGcmRenderer::SetFullViewport()
{
	SetViewport(
		Viewport_t(
			0.0f,
			0.0f,
			float32(display_width),
			float32(display_height)));
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

Viewport_t CGcmRenderer::GetViewport() const
{
	return m_Viewport;
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
	BufferUsage_t::Enum usage)
{
	RsxAllocation_t rsxAllocation = m_StaticHeap.Alloc(uint32(size), 64);
	if (!rsxAllocation.m_pPtr)
	{
		Error("[GCMRenderer] Failed to allocate vertex buffer memory\n");

		return 0;
	}

	if (pData)
	{
		memcpy(rsxAllocation.m_pPtr, pData, uint32(size));
	}
	else
	{
		memset(rsxAllocation.m_pPtr, 0, uint32(size));
	}

	BufferHandle hBuffer = m_NextHandle++;
	const BufferResource_t bufferResource = {
		rsxAllocation.m_pPtr,
		rsxAllocation.m_Offset,
		uint32(size),
		rsxAllocation
	};
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGcmRenderer::CreateIndexBuffer(
	const void* pData,
	uint64 size,
	IndexFormat_t::Enum format,
	BufferUsage_t::Enum usage)
{
	RsxAllocation_t rsxAllocation = m_StaticHeap.Alloc(uint32(size), 64);
	if (!rsxAllocation.m_pPtr)
	{
		Error("[GCMRenderer] Failed to allocate index buffer memory\n");

		return 0;
	}

	if (pData)
	{
		memcpy(rsxAllocation.m_pPtr, pData, uint32(size));
	}
	else
	{
		memset(rsxAllocation.m_pPtr, 0, uint32(size));
	}

	BufferHandle hBuffer = m_NextHandle++;
	const BufferResource_t bufferResource = {
		rsxAllocation.m_pPtr,
		rsxAllocation.m_Offset,
		uint32(size),
		rsxAllocation
	};
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGcmRenderer::CreateConstantBuffer(
	uint64 size,
	BufferUsage_t::Enum usage)
{
	RsxAllocation_t rsxAllocation = m_StaticHeap.Alloc(uint32(size), 64);
	if (!rsxAllocation.m_pPtr)
	{
		Error("[GCMRenderer] Failed to allocate constant buffer memory\n");

		return 0;
	}

	BufferHandle hBuffer = m_NextHandle++;
	const BufferResource_t bufferResource = {
		rsxAllocation.m_pPtr,
		rsxAllocation.m_Offset,
		uint32(size),
		rsxAllocation
	};
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

	__sync_synchronize();
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
	if (pPtr) m_StaticHeap.Free(m_BufferResources.Element(bufferIndex).m_Alloc);

	m_BufferResources.Remove(hBuffer);

	for (int32 i = m_InstanceCache.Count() - 1; i >= 0; i--)
	{
		if (m_InstanceCache[i].m_hVertexBuffer == hBuffer)
		{
			DestroyBuffer(m_InstanceCache[i].m_hExpandedBuffer);
			DestroyBuffer(m_InstanceCache[i].m_hInstanceBuffer);
			m_InstanceCache.RemoveAt(i);
		}
	}
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

	void* pVertexProgramAligned = memalign(
		64,
		uint32(vertexProgramBin.Count()));
	memcpy(
		pVertexProgramAligned,
		vertexProgramBin.Base(),
		uint32(vertexProgramBin.Count()));

	const rsxVertexProgram* pVertexProgram = reinterpret_cast<const rsxVertexProgram*>(
		pVertexProgramAligned);
	void* pVertexProgramUCode;
	uint32 vertexProgramSize;
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

		CUtlMemory::Free(pVertexProgramAligned);

		return 0;
	}

	RsxAllocation_t fragmentProgramAlloc = m_StaticHeap.Alloc(
		uint32(fragmentProgramBin.Count()), 64);
	if (!fragmentProgramAlloc.m_pPtr)
	{
		Warning(
			"[GCMRenderer] Failed to allocate fragment program memory\n");

		CUtlMemory::Free(pVertexProgramAligned);

		return 0;
	}
	void* pFragmentProgramAligned = fragmentProgramAlloc.m_pPtr;
	memcpy(
		pFragmentProgramAligned,
		fragmentProgramBin.Base(),
		uint32(fragmentProgramBin.Count()));

	const rsxFragmentProgram* pFragmentProgram = reinterpret_cast<const rsxFragmentProgram*>(
		pFragmentProgramAligned);
	void* pFragmentProgramCode;
	uint32 fragmentProgramSize;
	rsxFragmentProgramGetUCode(
		const_cast<rsxFragmentProgram*>(pFragmentProgram),
		&pFragmentProgramCode,
		&fragmentProgramSize);

	RsxAllocation_t fragmentProgramBufferAlloc = m_StaticHeap.Alloc(
		fragmentProgramSize, 64);
	if (!fragmentProgramBufferAlloc.m_pPtr)
	{
		Warning(
			"[GCMRenderer] Failed to allocate fragment program buffer memory\n");

		m_StaticHeap.Free(fragmentProgramAlloc);
		CUtlMemory::Free(pVertexProgramAligned);

		return 0;
	}
	void* pFragmentProgramBuffer = fragmentProgramBufferAlloc.m_pPtr;
	memcpy(pFragmentProgramBuffer, pFragmentProgramCode, fragmentProgramSize);
	const uint32 fragmentProgramOffset = fragmentProgramBufferAlloc.m_Offset;

	ShaderProgramHandle hProgram = m_NextHandle++;
	ProgramResource_t programResource;
	programResource.m_pVertexProgramAligned = pVertexProgramAligned;
	programResource.m_pVertexProgram = pVertexProgram;
	programResource.m_pVertexProgramUCode = pVertexProgramUCode;
	programResource.m_pFragmentProgramAligned = pFragmentProgramAligned;
	programResource.m_pFragmentProgram = pFragmentProgram;
	programResource.m_pFragmentProgramBuffer = pFragmentProgramBuffer;
	programResource.m_VertexProgramSize = vertexProgramSize;
	programResource.m_FragmentProgramOffset = fragmentProgramOffset;
	programResource.m_FragmentProgramSize = fragmentProgramSize;
	programResource.m_FragmentProgramAlloc = fragmentProgramAlloc;
	programResource.m_FragmentProgramBufferAlloc = fragmentProgramBufferAlloc;
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
	if (programResource.m_pVertexProgramAligned)
	{
		CUtlMemory::Free(programResource.m_pVertexProgramAligned);
	}
	if (programResource.m_pFragmentProgramAligned)
	{
		m_StaticHeap.Free(programResource.m_FragmentProgramAlloc);
	}
	if (programResource.m_pFragmentProgramBuffer)
	{
		m_StaticHeap.Free(programResource.m_FragmentProgramBufferAlloc);
	}

	m_ProgramResources.Remove(hProgram);
}

TextureHandle CGcmRenderer::CreateTexture2D(
	uint32 width,
	uint32 height,
	TextureFormat_t::Enum format,
	const void* pData)
{
	// 4 bytes per pixel (ARGB8)
	const uint32 textureBufferSize = width * height * 4;
	RsxAllocation_t rsxAllocation = m_StaticHeap.Alloc(textureBufferSize, 128);
	if (!rsxAllocation.m_pPtr)
	{
		Warning("[GCMRenderer] Failed to allocate texture buffer memory\n");

		return 0;
	}
	void* pTextureBuffer = rsxAllocation.m_pPtr;

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

	const TextureResource_t textureResource = {
		pTextureBuffer,
		textureOffset,
		width,
		height,
		format,
		rsxAllocation,
		false
	};
	const TextureHandle hTexture = m_NextHandle++;
	m_TextureResources.Insert(hTexture, textureResource);

	return hTexture;
}

TextureHandle CGcmRenderer::CreateTextureCube(
	uint32 size,
	TextureFormat_t::Enum format,
	const void** pFaces)
{
	// 6 faces, 4 bytes per pixel (ARGB8)
	const uint32 textureBufferSize = size * size * 4 * 6;
	RsxAllocation_t rsxAllocation = m_StaticHeap.Alloc(textureBufferSize, 128);
	if (!rsxAllocation.m_pPtr)
	{
		Warning("[GCMRenderer] Failed to allocate texture buffer memory\n");

		return 0;
	}
	void* pTextureBuffer = rsxAllocation.m_pPtr;

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

	const TextureResource_t textureResource = {
		pTextureBuffer,
		textureOffset,
		size,
		size,
		format,
		rsxAllocation,
		true
	};
	const TextureHandle hTexture = m_NextHandle++;
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

	uint32 gcmTextureFormat;
	uint32 texturePitch;

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
	memset(&gcmTex, 0, sizeof(gcmTexture));
	gcmTex.format = static_cast<uint8>(gcmTextureFormat);
	gcmTex.mipmap = 1;
	gcmTex.dimension = GCM_TEXTURE_DIMS_2D;
	gcmTex.cubemap = static_cast<uint8>(textureResource.m_IsCubemap ? GCM_TRUE : GCM_FALSE);
	gcmTex.remap = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
				  (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
				  (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
				  (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
				  (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
				  (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
				  (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
				  (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
	gcmTex.width = static_cast<uint16>(textureResource.m_Width);
	gcmTex.height = static_cast<uint16>(textureResource.m_Height);
	gcmTex.depth = static_cast<uint16>(textureResource.m_IsCubemap ? 6 : 1);
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
	if (pTextureBuffer) m_StaticHeap.Free(m_TextureResources.Element(textureIndex).m_Alloc);

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

	BoundUniform_t boundUniform = {
		hBuffer,
		slot,
		stage
	};
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

int32 CGcmRenderer::GetUniformBlockBinding(
	ShaderProgramHandle hProgram,
	const char* pBlockName)
{
	return -1;
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

			int32 constCacheIndex = constCache.Find(uniformName);
			rsxProgramConst* pConst = (constCacheIndex != constCache.InvalidIndex()) ?
				constCache.Element(constCacheIndex) :
				rsxVertexProgramGetConst(
					const_cast<rsxVertexProgram*>(
						programResource.m_pVertexProgram),
					uniformName.Get());

			if (constCacheIndex == constCache.InvalidIndex() && pConst)
			{
				constCache.Insert(uniformName, pConst);
			}

			if (!pConst)
			{
				Warning(
					"[GCMRenderer] Const: %s not found in prog: %u\n",
					uniformName.Get(),
					hProgram);

				continue;
			}

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
				reinterpret_cast<const float32*>(
					pShadowData + uint32(uniformNameIndex) * uniformBytes),
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
	const CMatrix4* pViewProjection,
	const CVector3* pAABBCenter,
	const CVector3* pAABBExtent)
{
	if (pViewProjection && pAABBCenter && pAABBExtent)
	{
		Plane_t frustumPlanes[6];
		ExtractFrustumPlanes(*pViewProjection, frustumPlanes);
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
	const CMatrix4* pViewProjection,
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

	if (pViewProjection && pAABBCenter && pAABBExtent)
	{
		Plane_t frustumPlanes[6];
		ExtractFrustumPlanes(*pViewProjection, frustumPlanes);
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

#ifdef SHADER_INSTANCING_ENABLED
void CGcmRenderer::DrawInstanced(
	uint32 vertexCount,
	uint32 instanceCount,
	const CMatrix4* pMatrices,
	const CVertexLayout* pInstanceLayout)
{
	if (instanceCount == 0 || !pInstanceLayout) return;

	const ShaderProgramHandle hProgram = m_PipelineState.m_hShaderProgram;
	if (!hProgram) return;

	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex()) return;

	const CUtlVector<VertexAttribute_t>& attributes = pInstanceLayout->GetAttributes();
	const uint32 instanceStride = pInstanceLayout->GetStride();
	StagingBuffer_t& instanceBuffer = m_InstanceBuffers[m_InstanceBufferIndex];
	const uint32 alignedSize = ((instanceCount * instanceStride) + 127) & ~127;

	if (instanceBuffer.m_Size < alignedSize)
	{
		if (instanceBuffer.m_pPtr) m_DynamicHeap.Free(instanceBuffer.m_Alloc);
		instanceBuffer.m_Alloc = m_DynamicHeap.Alloc(alignedSize, 128);
		instanceBuffer.m_pPtr = instanceBuffer.m_Alloc.m_pPtr;
		if (!instanceBuffer.m_pPtr)
		{
			Warning("[GCMRenderer] Staging heap OOM for instance buffer\n");

			return;
		}
		instanceBuffer.m_Size = alignedSize;
	}

	float32* pInst = reinterpret_cast<float32*>(instanceBuffer.m_pPtr);
	for (uint32 i = 0; i < instanceCount; i++)
	{
		const CMatrix4& matrix = pMatrices[i];
		for (uint32 row = 0; row < 4; row++)
		{
			*pInst++ = matrix.m_Data[row];
			*pInst++ = matrix.m_Data[4 + row];
			*pInst++ = matrix.m_Data[8 + row];
			*pInst++ = matrix.m_Data[12 + row];
		}
	}

	__sync_synchronize();

	FlushPipelineState();

	ApplyVertexConstants(hProgram);
	ApplyFragmentConstants(hProgram);

	rsxSetFrequencyDividerOperation(context, GCM_FREQUENCY_DIVIDE);

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_VertexSemantic == VertexSemantic_t::Unspecified)
		{
			continue;
		}

		const uint32 attributeLocation = GetVertexSemanticAttributeIndex(
			attribute.m_VertexSemantic);
		if (attributeLocation == 0xFFFFFFFF) continue;

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			uint16(vertexCount),
			instanceBuffer.m_Alloc.m_Offset + attribute.m_Offset,
			uint8(instanceStride),
			4,
			GCM_VERTEX_DATA_TYPE_F32,
			GCM_LOCATION_RSX);
	}

	rsxInvalidateVertexCache(context);
	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);

	rsxDrawVertexArray(
		context,
		GCM_TYPE_TRIANGLES,
		0,
		instanceCount * vertexCount);

	rsxSetFrequencyDividerOperation(context, GCM_FREQUENCY_MODULO);

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_VertexSemantic == VertexSemantic_t::Unspecified)
		{
			continue;
		}

		const uint32 attributeLocation = GetVertexSemanticAttributeIndex(
			attribute.m_VertexSemantic);
		if (attributeLocation == 0xFFFFFFFF) continue;

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			0, 0, 0, 4,
			GCM_VERTEX_DATA_TYPE_F32,
			GCM_LOCATION_RSX);
	}

	rsxFlushBuffer(context);

	m_InstanceBufferIndex = (m_InstanceBufferIndex + 1) % s_MaxInstanceStagingBuffers;
}

void CGcmRenderer::DrawIndexedInstanced(
	uint32 indexCount,
	uint32 instanceCount,
	const CMatrix4* pMatrices,
	uint32 startIndex,
	const CVertexLayout* pInstanceLayout)
{
	if (instanceCount == 0 || !pInstanceLayout || !pMatrices) return;

	const ShaderProgramHandle hProgram = m_PipelineState.m_hShaderProgram;
	if (!hProgram) return;

	const BufferHandle hVertexBuffer = m_PipelineState.m_hVertexBuffer;
	const BufferHandle hIndexBuffer = m_PipelineState.m_hIndexBuffer;
	const uint32 vertexStride = m_PipelineState.m_VertexStride;
	if (!hVertexBuffer || !hIndexBuffer) return;

	int32 cacheIndex = -1;
	for (int32 i = 0; i < m_InstanceCache.Count(); i++)
	{
		const InstanceCache_t& instanceCache = m_InstanceCache[i];
		if (instanceCache.m_pMatrices == pMatrices &&
			instanceCache.m_hVertexBuffer == hVertexBuffer &&
			instanceCache.m_InstanceCount == instanceCount)
		{
			cacheIndex = i;

			break;
		}
	}

	if (cacheIndex == -1)
	{
		int32 vertexBufferIndex = m_BufferResources.Find(hVertexBuffer);
		int32 indexBufferIndex = m_BufferResources.Find(hIndexBuffer);
		if (vertexBufferIndex == m_BufferResources.InvalidIndex() ||
			indexBufferIndex == m_BufferResources.InvalidIndex()) return;

		const uint32 expandedSize = instanceCount * indexCount * vertexStride;
		const uint32 alignedSize = (expandedSize + 4096 + 127) & ~127;
		RsxAllocation_t expandedAlloc = m_StaticHeap.Alloc(alignedSize, 128);
		if (!expandedAlloc.m_pPtr) return;
		void* pExpanded = expandedAlloc.m_pPtr;

		const uint8* pSrc = reinterpret_cast<const uint8*>(
			m_BufferResources.Element(vertexBufferIndex).m_pPtr);
		const uint32* pIndices = reinterpret_cast<const uint32*>(
			m_BufferResources.Element(indexBufferIndex).m_pPtr);
		uint8* pDst = reinterpret_cast<uint8*>(pExpanded);
		for (uint32 i = 0; i < instanceCount; i++)
		{
			for (uint32 j = 0; j < indexCount; j++)
			{
				memcpy(
					pDst + (i * indexCount + j) * vertexStride,
					pSrc + pIndices[j] * vertexStride,
					vertexStride);
			}
		}

		__sync_synchronize();

		BufferResource_t expandedResource;
		expandedResource.m_pPtr = pExpanded;
		expandedResource.m_Offset = expandedAlloc.m_Offset;
		expandedResource.m_Size = expandedSize;
		expandedResource.m_Alloc = expandedAlloc;
		const BufferHandle hExpanded = AllocHandle();
		m_BufferResources.Insert(hExpanded, expandedResource);

		const uint32 instanceStride = pInstanceLayout->GetStride();
		const uint32 instanceDataSize = instanceCount * instanceStride;
		const uint32 instanceDataAlignedSize = (instanceDataSize + 127) & ~127;
		RsxAllocation_t instanceDataAlloc = m_StaticHeap.Alloc(
			instanceDataAlignedSize, 128);
		if (!instanceDataAlloc.m_pPtr) return;
		void* pInstanceData = instanceDataAlloc.m_pPtr;

		float32* pInst = reinterpret_cast<float32*>(pInstanceData);
		for (uint32 i = 0; i < instanceCount; i++)
		{
			const CMatrix4& matrix = pMatrices[i];
			for (uint32 row = 0; row < 4; row++)
			{
				*pInst++ = matrix.m_Data[row];
				*pInst++ = matrix.m_Data[4 + row];
				*pInst++ = matrix.m_Data[8 + row];
				*pInst++ = matrix.m_Data[12 + row];
			}
		}

		__sync_synchronize();

		BufferResource_t instanceDataResource;
		instanceDataResource.m_pPtr = pInstanceData;
		instanceDataResource.m_Offset = instanceDataAlloc.m_Offset;
		instanceDataResource.m_Size = instanceDataSize;
		instanceDataResource.m_Alloc = instanceDataAlloc;
		const BufferHandle hInstanceBuffer = AllocHandle();
		m_BufferResources.Insert(hInstanceBuffer, instanceDataResource);

		InstanceCache_t instanceCache;
		instanceCache.m_pMatrices = pMatrices;
		instanceCache.m_hVertexBuffer = hVertexBuffer;
		instanceCache.m_hExpandedBuffer = hExpanded;
		instanceCache.m_hInstanceBuffer = hInstanceBuffer;
		instanceCache.m_InstanceCount = instanceCount;
		cacheIndex = m_InstanceCache.AddToTail(instanceCache);
	}

	const InstanceCache_t& instanceCache = m_InstanceCache[cacheIndex];

	int32 instanceBufferIndex = m_BufferResources.Find(
		instanceCache.m_hInstanceBuffer);
	if (m_BufferResources.Find(instanceCache.m_hExpandedBuffer) ==
		m_BufferResources.InvalidIndex() ||
		instanceBufferIndex == m_BufferResources.InvalidIndex()) return;

	const BufferResource_t& instanceResource =
		m_BufferResources.Element(instanceBufferIndex);
	const CUtlVector<VertexAttribute_t>& attributes =
		pInstanceLayout->GetAttributes();
	const uint32 instanceStride = pInstanceLayout->GetStride();

	const BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	m_PipelineState.m_hVertexBuffer = instanceCache.m_hExpandedBuffer;
	FlushPipelineState();
	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	ApplyVertexConstants(hProgram);
	ApplyFragmentConstants(hProgram);

	rsxSetFrequencyDividerOperation(context, GCM_FREQUENCY_DIVIDE);

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_VertexSemantic == VertexSemantic_t::Unspecified)
		{
			continue;
		}

		const uint32 attributeLocation =
			GetVertexSemanticAttributeIndex(attribute.m_VertexSemantic);
		if (attributeLocation == 0xFFFFFFFF) continue;

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			uint16(indexCount),
			instanceResource.m_Offset + attribute.m_Offset,
			uint8(instanceStride),
			4,
			GCM_VERTEX_DATA_TYPE_F32,
			GCM_LOCATION_RSX);
	}

	rsxInvalidateVertexCache(context);
	rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
	rsxDrawVertexArray(
		context,
		GCM_TYPE_TRIANGLES,
		0,
		instanceCount * indexCount);

	rsxSetFrequencyDividerOperation(context, GCM_FREQUENCY_MODULO);

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_VertexSemantic == VertexSemantic_t::Unspecified)
		{
			continue;
		}

		const uint32 attributeLocation =
			GetVertexSemanticAttributeIndex(attribute.m_VertexSemantic);
		if (attributeLocation == 0xFFFFFFFF) continue;

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			0,
			0,
			0,
			4,
			GCM_VERTEX_DATA_TYPE_F32,
			GCM_LOCATION_RSX);
	}

	rsxFlushBuffer(context);
}

#else // !SHADER_INSTANCING_ENABLED
void CGcmRenderer::DrawInstanced(
	uint32 vertexCount,
	uint32 instanceCount,
	const CMatrix4* pMatrices,
	const CVertexLayout* pInstanceLayout)
{
	Warning("[GCMRenderer] DrawInstanced not implemented\n");
}
void CGcmRenderer::DrawIndexedInstanced(
	uint32 indexCount,
	uint32 instanceCount,
	const CMatrix4* pMatrices,
	uint32 startIndex,
	const CVertexLayout* pInstanceLayout)
{
	Warning("[GCMRenderer] DrawIndexedInstanced not implemented\n");
}
#endif // !SHADER_INSTANCING_ENABLED

uint32 CGcmRenderer::GetVertexSemanticAttributeIndex(
	VertexSemantic_t::Enum vertexSemantic)
{
	switch (vertexSemantic)
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
	uint32 vertexStride,
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

	const CUtlVector<VertexAttribute_t>& attributes = pLayout->GetAttributes();
	for (int32 attributeIndex = 0; attributeIndex < attributes.Count(); attributeIndex++)
	{
		const VertexAttribute_t& attribute = attributes[attributeIndex];
		if (attribute.m_Name.IsEmpty()) continue;

		void* pAttributePtr = reinterpret_cast<char*>(
			m_BufferResources.Element(vertexBufferIndex).m_pPtr) + offset + attribute.m_Offset;
		uint32 attributeLocation = 0xFFFFFFFF;
		uint32 attributeOffset;
		uint32 components;
		uint8 dataType;

		if (attribute.m_VertexSemantic != VertexSemantic_t::Unspecified)
		{
			attributeLocation = GetVertexSemanticAttributeIndex(
				attribute.m_VertexSemantic);
		}

		if (attributeLocation == 0xFFFFFFFF) continue;

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

		rsxAddressToOffset(pAttributePtr, &attributeOffset);

		rsxBindVertexArrayAttrib(
			context,
			attributeLocation,
			0,
			attributeOffset,
			uint16(vertexStride ? vertexStride : pLayout->GetStride()),
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

#ifdef PS3_SPU_ENABLED
void CGcmRenderer::MarkUniformsDirty(ShaderProgramHandle hProgram)
{
	int32 uniformShadowsIndex = m_ProgramUniformShadows.Find(hProgram);
	if (uniformShadowsIndex == m_ProgramUniformShadows.InvalidIndex())
	{
		return;
	}

	CUtlMap<uint32, UniformShadow_t>& uniformShadows = m_ProgramUniformShadows.Element(
		uniformShadowsIndex);
	for (int32 i = uniformShadows.FirstInorder(); uniformShadows.IsValidIndex(i); i = uniformShadows.NextInorder(i))
	{
		uniformShadows.Element(i).m_IsDirty = true;
	}
}
#endif // PS3_SPU_ENABLED

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