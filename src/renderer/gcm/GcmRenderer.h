//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GCM_RENDERER_H
#define GCM_RENDERER_H

#pragma once

#include "renderer/Renderer.h"
#include "rsxutil/rsxutil.h"
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <rsx/commands.h>
#include <rsx/rsx_program.h>

#ifdef PS3_SPU_ENABLED
class CSpuBatchTransformManager;
#endif

class CGcmRenderer : public virtual CRenderer
{
public:
	CGcmRenderer();
	virtual ~CGcmRenderer() GCMGL_OVERRIDE;

	virtual bool Init(const RendererDesc_t& rendererDesc) GCMGL_OVERRIDE;
	virtual void Shutdown() GCMGL_OVERRIDE;

	virtual void SetEnvironment() GCMGL_OVERRIDE;
	virtual void BeginFrame() GCMGL_OVERRIDE;
	virtual void EndFrame() GCMGL_OVERRIDE;

	// Clear
	virtual void Clear(
		uint32 clearFlags,
		const CColor& color = CColor::Black,
		float32 depth = 1.0f,
		uint32 stencil = 0) GCMGL_OVERRIDE;

	// Viewport & targets
	virtual void SetViewport(const Viewport_t& viewport) GCMGL_OVERRIDE;
	virtual void SetScissor(const Rect_t& rect) GCMGL_OVERRIDE;
	virtual void SetStencilRef(uint32 stencilRef) GCMGL_OVERRIDE;

	// Buffers
	virtual BufferHandle CreateVertexBuffer(
		const void* pData,
		uint64 size,
		BufferUsage_t::Enum usage) GCMGL_OVERRIDE;
	virtual BufferHandle CreateIndexBuffer(
		const void* pData,
		uint64 size,
		IndexFormat_t::Enum format,
		BufferUsage_t::Enum usage) GCMGL_OVERRIDE;
	virtual BufferHandle CreateConstantBuffer(
		uint64 size,
		BufferUsage_t::Enum usage) GCMGL_OVERRIDE;
	virtual void UpdateBuffer(
		BufferHandle hBuffer,
		const void* pData,
		uint64 size,
		uint64 offset = 0) GCMGL_OVERRIDE;
	virtual void DestroyBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;
	virtual void* MapBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;
	virtual void UnmapBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;
	virtual BufferHandle BuildInstancedVertexBuffer(
		BufferHandle hVertexBuffer,
		BufferHandle hIndexBuffer,
		uint32 indexCount,
		uint32 instanceCount,
		uint32 vertexStride) GCMGL_OVERRIDE;

	// Shaders
	virtual ShaderProgramHandle CreateShaderProgram(
		const CFixedString& shaderName) GCMGL_OVERRIDE;
	virtual void DestroyShaderProgram(
		ShaderProgramHandle hProgram) GCMGL_OVERRIDE;

	// Textures
	virtual TextureHandle CreateTexture2D(
		uint32 width,
		uint32 height,
		TextureFormat_t::Enum format,
		const void* pData = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual TextureHandle CreateTextureCube(
		uint32 size,
		TextureFormat_t::Enum format,
		const void** pFaces = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void SetTexture(
		TextureHandle hTexture,
		uint32 slot,
		ShaderStage_t stage) GCMGL_OVERRIDE;
	virtual void SetSampler(
		SamplerHandle hSampler,
		uint32 slot,
		ShaderStage_t stage) GCMGL_OVERRIDE;
	virtual void UpdateTexture(
		TextureHandle hTexture,
		const void* pData,
		uint32 mipLevel = 0) GCMGL_OVERRIDE;
	virtual void DestroyTexture(TextureHandle hTexture) GCMGL_OVERRIDE;

	// Render state
	virtual void SetConstantBuffer(
		BufferHandle hBuffer,
		UniformBlockLayoutHandle hLayout,
		uint32 slot,
		ShaderStage_t stage) GCMGL_OVERRIDE;

	virtual void SetBlendState(const BlendState_t& state) GCMGL_OVERRIDE;
	virtual void SetDepthStencilState(
		const DepthStencilState_t& state) GCMGL_OVERRIDE;

	virtual void ApplyVertexConstants(
		ShaderProgramHandle hProgram) GCMGL_OVERRIDE;
	virtual void ApplyFragmentConstants(
		ShaderProgramHandle hProgram) GCMGL_OVERRIDE;

	// Drawing
	virtual void Draw(
		uint32 vertexCount,
		uint32 startVertex = 0,
		const CMatrix4* pViewProjection = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void DrawIndexed(
		uint32 indexCount,
		uint32 startIndex = 0,
		int32 baseVertex = 0,
		const CMatrix4* pViewProjection = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void DrawInstanced(
		uint32 vertexCount,
		uint32 instanceCount,
		const CMatrix4* pMatrices,
		const CVertexLayout* pInstanceLayout = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void DrawIndexedInstanced(
		uint32 indexCount,
		uint32 instanceCount,
		const CMatrix4* pMatrices,
		uint32 startIndex = 0,
		const CVertexLayout* pInstanceLayout = GCMGL_NULL) GCMGL_OVERRIDE;

	// Pipeline
	uint32 GetVertexSemanticAttributeIndex(
		VertexSemantic_t::Enum vertexSemantic);
	virtual void BindVertexAttributes(
		const CVertexLayout* pLayout,
		uint32 vertexStride,
		uint32 offset) GCMGL_OVERRIDE;
	virtual void FlushProgramState() GCMGL_OVERRIDE;
protected:
#ifdef PS3_SPU_ENABLED
	void MarkUniformsDirty(ShaderProgramHandle hProgram);
#endif

	struct BufferResource_t
	{
		void* m_pPtr;
		uint32 m_Offset;
		uint32 m_Size;
	};

	struct StagingBuffer_t
	{
		StagingBuffer_t() : 
			m_pPtr(GCMGL_NULL), 
			m_Offset(0), 
			m_Size(0), 
			m_hBuffer(0)
		{
		}

		void* m_pPtr;
		uint32 m_Offset;
		uint32 m_Size;
		BufferHandle m_hBuffer;
	};

	StagingBuffer_t m_StagingVertexBuffer[2];
	StagingBuffer_t m_StagingIndexBuffer[2];
	StagingBuffer_t m_InstanceBuffer;
	CUtlMap<BufferHandle, BufferResource_t> m_BufferResources;
#ifdef PS3_SPU_ENABLED
	CSpuBatchTransformManager* m_pSpuBatchTransformManager;
#endif
	int32 m_StagingIndex;
protected:
	struct ProgramResource_t
	{
		CUtlMap<CFixedString, rsxProgramConst*> m_VertexProgramConstCache;
		CUtlMap<CFixedString, rsxProgramConst*> m_FragmentProgramConstCache;
		void* m_pVertexProgramAligned;
		const rsxVertexProgram* m_pVertexProgram;
		void* m_pVertexProgramUCode;
		void* m_pFragmentProgramAligned;
		const rsxFragmentProgram* m_pFragmentProgram;
		void* m_pFragmentProgramBuffer;
		uint32 m_VertexProgramSize;
		uint32 m_FragmentProgramOffset;
		uint32 m_FragmentProgramSize;
	};

	CUtlMap<ShaderProgramHandle, ProgramResource_t> m_ProgramResources;
private:
	struct TextureResource_t
	{
		void* m_pBuffer;
		uint32 m_Offset;
		uint32 m_Width;
		uint32 m_Height;
		TextureFormat_t::Enum m_Format;
		bool m_IsCubemap;
	};

	struct BoundUniform_t
	{
		BufferHandle m_hBuffer;
		uint32 m_Slot;
		ShaderStage_t m_Stage;
	};

	struct UniformShadow_t
	{
		CUtlVector<uint8> m_Data;
		bool m_IsDirty;
	};

	CUtlMap<ShaderProgramHandle, CUtlMap<uint32, UniformShadow_t> > m_ProgramUniformShadows;
	CUtlMap<ShaderProgramHandle, CUtlMap<uint32, BoundUniform_t> > m_ProgramUniformBuffers;
	CUtlMap<TextureHandle, TextureResource_t> m_TextureResources;

	Viewport_t m_Viewport;
	float32 m_ViewportScale[4];
	float32 m_ViewportOffset[4];

	void* m_pHostAddr;
	uint32 m_HostSize;
};

#endif // GCM_RENDERER_H