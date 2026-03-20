//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#pragma once

#include "renderer/Renderer.h"
#include <glad/gl.h>

struct GLFWwindow;

class CGlRenderer : public virtual CRenderer
{
public:
	CGlRenderer();
	virtual ~CGlRenderer() GCMGL_OVERRIDE;

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
		BufferUsage_t usage) GCMGL_OVERRIDE;
	virtual BufferHandle CreateIndexBuffer(
		const void* pData,
		uint64 size,
		IndexFormat_t format,
		BufferUsage_t usage) GCMGL_OVERRIDE;
	virtual BufferHandle CreateConstantBuffer(
		uint64 size,
		BufferUsage_t usage) GCMGL_OVERRIDE;
	virtual void UpdateBuffer(
		BufferHandle hBuffer,
		const void* pData,
		uint64 size,
		uint64 offset = 0) GCMGL_OVERRIDE;
	virtual void DestroyBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;
	virtual void* MapBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;
	virtual void UnmapBuffer(BufferHandle hBuffer) GCMGL_OVERRIDE;

	// Shaders
	virtual ShaderProgramHandle CreateShaderProgram(
		const CFixedString& shaderName) GCMGL_OVERRIDE;
	virtual void DestroyShaderProgram(
		ShaderProgramHandle hProgram) GCMGL_OVERRIDE;

	// Textures
	virtual TextureHandle CreateTexture2D(
		uint32 width,
		uint32 height,
		TextureFormat_t format,
		const void* pData = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual TextureHandle CreateTextureCube(
		uint32 size,
		TextureFormat_t format,
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
		const CMatrix4* pViewProj = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void DrawIndexed(
		uint32 indexCount,
		uint32 startIndex = 0,
		int32 baseVertex = 0,
		const CMatrix4* pViewProj = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) GCMGL_OVERRIDE;

	// Pipeline
	uint32 GetSemanticAttributeIndex(VertexSemantic_t semantic);
	virtual void BindVertexAttributes(
		const CVertexLayout* pLayout,
		uint32 stride,
		uint32 offset) GCMGL_OVERRIDE;
	virtual void FlushProgramState() GCMGL_OVERRIDE;
protected:
	struct BufferResource_t
	{
		void* m_pPtr;
		uint32 m_hId;
		uint64 m_Size;
		uint32 m_Target;
		bool m_IsAligned;
	};

	struct StagingBuffer_t
	{
		CUtlVector<uint8> m_Data;
		uint32 m_hId;
		BufferHandle m_hBuffer;
	};

	CUtlMap<BufferHandle, BufferResource_t> m_BufferResources;

	StagingBuffer_t m_StagingVertexBuffer[2];
	StagingBuffer_t m_StagingIndexBuffer[2];
	int32 m_StagingIndex;
private:
	struct ProgramResource_t
	{
		uint32 m_hId;
		CUtlMap<CFixedString, int32> m_AttributeLocations;
		CUtlMap<CFixedString, int32> m_UniformLocations;
	};

	struct TextureResource_t
	{
		uint32 m_hId;
		uint32 m_Target;
		uint32 m_Width;
		uint32 m_Height;
		TextureFormat_t m_Format;
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
private:
	GLFWwindow* m_pWindow;

	Viewport_t m_Viewport;
	float32 m_ViewportScale[4];
	float32 m_ViewportOffset[4];

	CUtlMap<ShaderProgramHandle, ProgramResource_t> m_ProgramResources;
	CUtlMap<TextureHandle, TextureResource_t> m_TextureResources;

	CUtlMap<ShaderProgramHandle, CUtlMap<uint32, BoundUniform_t>> m_ProgramUniformBuffers;
	CUtlMap<ShaderProgramHandle, CUtlMap<uint32, UniformShadow_t>> m_ProgramUniformShadows;
};

#endif // GL_RENDERER_H