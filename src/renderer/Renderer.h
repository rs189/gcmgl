//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef RENDERER_H
#define RENDERER_H

#pragma once

#include "tier0/platform.h"
#include "utils/Color.h"
#include "mathsfury/Matrix4.h"
#include "mathsfury/Vector3.h"
#include "mathsfury/Quaternion.h"
#include "mathsfury/Maths.h"
#include "utils/UtlVector.h"
#include "utils/UtlMap.h"
#include "utils/FixedString.h"

typedef uint32 BufferHandle;
typedef uint32 ShaderProgramHandle;
typedef uint32 MaterialHandle;
typedef uint32 TextureHandle;
typedef uint32 SamplerHandle;
typedef uint32 RenderTargetHandle;
typedef uint32 UniformBlockLayoutHandle;

struct RendererDesc_t
{
	void* m_pWindow;
	uint32 m_Width;
	uint32 m_Height;
	bool m_IsFullscreen;
	bool m_IsVSync;
};

enum ClearFlags_t
{
	ClearNone = 0,
	ClearColor = 1 << 0,
	ClearDepth = 1 << 1,
	ClearStencil = 1 << 2,
	ClearAll = ClearColor | ClearDepth | ClearStencil
};

struct Viewport_t
{
	float32 m_X;
	float32 m_Y;
	float32 m_Width;
	float32 m_Height;
	float32 m_MinDepth;
	float32 m_MaxDepth;

	Viewport_t() :
		m_X(0.0f),
		m_Y(0.0f),
		m_Width(1920.0f),
		m_Height(1080.0f),
		m_MinDepth(0.0f),
		m_MaxDepth(1.0f)
	{
	}

	Viewport_t(
		float32 x,
		float32 y,
		float32 w,
		float32 h,
		float32 minD = 0.0f,
		float32 maxD = 1.0f) :
		m_X(x),
		m_Y(y),
		m_Width(w),
		m_Height(h),
		m_MinDepth(minD),
		m_MaxDepth(maxD)
	{
	}
};

struct Rect_t
{
	int32 m_X;
	int32 m_Y;
	uint32 m_Width;
	uint32 m_Height;

	Rect_t() :
		m_X(0),
		m_Y(0),
		m_Width(1920),
		m_Height(1080)
	{
	}

	Rect_t(int32 x, int32 y, uint32 w, uint32 h) :
		m_X(x),
		m_Y(y),
		m_Width(w),
		m_Height(h)
	{
	}
};

struct BufferUsage_t
{
	enum Enum
	{
		Static,
		Dynamic,
		Immutable
	};
};

struct IndexFormat_t
{
	enum Enum
	{
		UInt16,
		UInt32
	};
};

struct TextureFormat_t
{
	enum Enum
	{
		R8,
		RG8,
		RGB8,
		RGBA8,
		R16F,
		RG16F,
		RGB16F,
		RGBA16F,
		R32F,
		RG32F,
		RGB32F,
		RGBA32F,
		Depth16,
		Depth24,
		Depth32F,
		Depth24Stencil8
	};
};

enum ShaderStage_t
{
	ShaderStageVertex = 1 << 0,
	ShaderStageFragment = 1 << 1,
	ShaderStageAll = ShaderStageVertex | ShaderStageFragment
};

struct VertexFormat_t
{
	enum Enum
	{
		Unspecified = 0,
		Float = 1,
		Float2 = 2,
		Float3 = 3,
		Float4 = 4,
		UByte4_Norm = 5
	};
};

struct VertexSemantic_t
{
	enum Enum
	{
		Unspecified = 0,
		Position = 1,
		Weight = 2,
		Normal = 3,
		Color0 = 4,
		Color1 = 5,
		Fog = 6,
		PointSize = 7,
		EdgeFlag = 8,
		TexCoord0 = 9,
		TexCoord1 = 10,
		TexCoord2 = 11,
		TexCoord3 = 12,
		TexCoord4 = 13,
		TexCoord5 = 14,
		TexCoord6 = 15,
		Tangent = 16,
		TexCoord7 = 17,
		Binormal = 18
	};
};

struct VertexAttribute_t
{
	CFixedString m_Name;
	VertexFormat_t::Enum m_Format;
	uint32 m_Offset;
	uint32 m_Location;
	VertexSemantic_t::Enum m_VertexSemantic;
};

class CVertexLayout
{
public:
	CVertexLayout();

	void AddAttribute(
		const CFixedString& name,
		uint32 format,
		uint32 offset,
		uint32 location = 0);
	void AddAttribute(
		const CFixedString& name,
		uint32 format,
		uint32 offset,
		VertexSemantic_t::Enum vertexSemantic,
		uint32 location = 0);

	void SetStride(uint32 vertexStride);

	uint32 GetStride() const
	{
		return m_VertexStride;
	}

	const CUtlVector<VertexAttribute_t>& GetAttributes() const
	{
		return m_Attributes;
	}
private:
	CUtlVector<VertexAttribute_t> m_Attributes;
	uint32 m_VertexStride;
};

struct BlendState_t
{
	bool m_IsEnabled;
};

struct DepthStencilState_t
{
	bool m_IsDepthTest;
	bool m_IsDepthWrite;
};

struct PipelineState_t
{
	uint64 m_IndexOffset;
	const CVertexLayout* m_pVertexLayout;
	ShaderProgramHandle m_hShaderProgram;
	BufferHandle m_hVertexBuffer;
	BufferHandle m_hIndexBuffer;
	uint32 m_VertexStride;
	uint32 m_VertexOffset;
	DepthStencilState_t m_DepthStencilState;
	BlendState_t m_BlendState;

	bool operator==(const PipelineState_t& other) const;
};

struct UniformBlockLayout_t
{
	CUtlVector<CFixedString> m_UniformNames;
	uint32 m_Binding;
	uint32 m_Size;
};

struct ALIGN16 BatchChunkTransform_t
{
	CQuaternion m_Rotation;
	CVector3 m_Position;
	CVector3 m_Scale;

	BatchChunkTransform_t() :
		m_Rotation(),
		m_Position(0.0f, 0.0f, 0.0f),
		m_Scale(1.0f, 1.0f, 1.0f)
	{
	}

	BatchChunkTransform_t(
		const CVector3& position,
		const CQuaternion& rotation,
		const CVector3& scale) :
		m_Rotation(rotation),
		m_Position(position),
		m_Scale(scale)
	{
	}

	CMatrix4 ToMatrix() const
	{
		return m_Rotation.ToTransformMatrix(m_Position, m_Scale);
	}
};

struct BatchChunk_t
{
	CUtlVector<BatchChunkTransform_t> m_BatchChunkTransforms;
	CVector3 m_Center;
	CVector3 m_Extent;
};

class CBatch
{
public:
	CUtlVector<BatchChunk_t> m_BatchChunks;

	CBatch() :
		m_pCameraPos(GCMGL_NULL)
	{
	}

	void Add(
		const CVector3& position,
		const CQuaternion& rotation = CQuaternion(),
		const CVector3& scale = CVector3(1.0f, 1.0f, 1.0f))
	{
		m_BatchChunkTransforms.AddToTail(
			BatchChunkTransform_t(position, rotation, scale));
	}

	void Build(float32 chunkSize = 500.0f)
	{
		m_BatchChunks.RemoveAll();
		if (m_BatchChunkTransforms.Count() == 0) return;

		CVector3 minBounds = m_BatchChunkTransforms[0].m_Position;
		CVector3 maxBounds = minBounds;
		for (int32 i = 1; i < m_BatchChunkTransforms.Count(); i++)
		{
			const CVector3& position = m_BatchChunkTransforms[i].m_Position;
			if (position.m_X < minBounds.m_X) minBounds.m_X = position.m_X;
			if (position.m_X > maxBounds.m_X) maxBounds.m_X = position.m_X;
			if (position.m_Y < minBounds.m_Y) minBounds.m_Y = position.m_Y;
			if (position.m_Y > maxBounds.m_Y) maxBounds.m_Y = position.m_Y;
			if (position.m_Z < minBounds.m_Z) minBounds.m_Z = position.m_Z;
			if (position.m_Z > maxBounds.m_Z) maxBounds.m_Z = position.m_Z;
		}

		const int32 numChunksX = int32(
			(maxBounds.m_X - minBounds.m_X) / chunkSize) + 1;
		const int32 numChunksY = int32(
			(maxBounds.m_Y - minBounds.m_Y) / chunkSize) + 1;
		const int32 numChunksZ = int32(
			(maxBounds.m_Z - minBounds.m_Z) / chunkSize) + 1;
		const int32 numChunks = numChunksX * numChunksY * numChunksZ;
		m_BatchChunks.SetCount(numChunks);
		for (int32 y = 0; y < numChunksY; y++)
		{
			for (int32 z = 0; z < numChunksZ; z++)
			{
				for (int32 x = 0; x < numChunksX; x++)
				{
					BatchChunk_t& batchChunk = m_BatchChunks[y * numChunksX * numChunksZ + z * numChunksX + x];
					batchChunk.m_Center = CVector3(
						minBounds.m_X + (x + 0.5f) * chunkSize,
						minBounds.m_Y + (y + 0.5f) * chunkSize,
						minBounds.m_Z + (z + 0.5f) * chunkSize);
					batchChunk.m_Extent = CVector3(
						chunkSize * 0.5f,
						chunkSize * 0.5f,
						chunkSize * 0.5f);
					batchChunk.m_BatchChunkTransforms.RemoveAll();
				}
			}
		}

		for (int32 i = 0; i < m_BatchChunkTransforms.Count(); i++)
		{
			const CVector3& position = m_BatchChunkTransforms[i].m_Position;
			const int32 x = CMaths::Max(0, CMaths::Min(numChunksX - 1,
				int32((position.m_X - minBounds.m_X) / chunkSize)));
			const int32 y = CMaths::Max(0, CMaths::Min(numChunksY - 1,
				int32((position.m_Y - minBounds.m_Y) / chunkSize)));
			const int32 z = CMaths::Max(0, CMaths::Min(numChunksZ - 1,
				int32((position.m_Z - minBounds.m_Z) / chunkSize)));
			m_BatchChunks[y * numChunksX * numChunksZ + z * numChunksX + x].m_BatchChunkTransforms.AddToTail(
				m_BatchChunkTransforms[i]);
		}
		m_BatchChunkTransforms.RemoveAll();
	}

	void Clear()
	{
		m_BatchChunks.RemoveAll();
		m_BatchChunkTransforms.RemoveAll();
	}

	uint32 GetCount() const
	{
		uint32 count = 0;
		for (int32 i = 0; i < m_BatchChunks.Count(); i++)
		{
			count += uint32(m_BatchChunks[i].m_BatchChunkTransforms.Count());
		}

		return count;
	}
private:
	CUtlVector<BatchChunkTransform_t> m_BatchChunkTransforms;
public:
	const CVector3* m_pCameraPos;
};

struct BatchThreadData_t
{
	char* m_pDstVertexData;
	uint32* m_pDstIndexData;
	const char* m_pSrcVertexData;
	const uint32* m_pSrcIndices;
	const CUtlVector<BatchChunkTransform_t>* m_pBatchChunkTransforms;
	uint32 m_ThreadStart;
	uint32 m_ThreadEnd;
	uint32 m_ChunkStart;
	uint32 m_VertexCount;
	uint32 m_IndexCount;
	uint32 m_VertexLayoutStride;
	uint32 m_VertexPosOffset;
	bool m_HasVertexPos;
};

struct Plane_t
{
	CVector3 m_Normal;
	float32 m_Distance;

	Plane_t() :
		m_Normal(0.0f, 0.0f, 0.0f),
		m_Distance(0.0f)
	{
	}
};

struct CullThreadData_t
{
	const BatchChunkTransform_t* m_pSrcTransforms;
	CUtlVector<BatchChunkTransform_t>* m_pDstTransforms;
	const Plane_t* m_pPlanes;
	uint32 m_Start;
	uint32 m_End;
};

struct FrustumVisibility_t
{
	enum Enum
	{
		Outside,
		Partial,
		Inside
	};
};

struct StateDirtyFlags_t
{
	enum Enum
	{
		None = 0,
		Program = 1 << 0,
		VertexBuffer = 1 << 1,
		IndexBuffer = 1 << 2,
		BlendState = 1 << 3,
		DepthStencilState = 1 << 4,
		Uniforms = 1 << 5,
		All = 0xFFFFFFFF
	};
};

INLINE StateDirtyFlags_t::Enum operator|(
	StateDirtyFlags_t::Enum a,
	StateDirtyFlags_t::Enum b)
{
	return static_cast<StateDirtyFlags_t::Enum>(uint32(a) | uint32(b));
}

INLINE StateDirtyFlags_t::Enum operator&(
	StateDirtyFlags_t::Enum a,
	StateDirtyFlags_t::Enum b)
{
	return static_cast<StateDirtyFlags_t::Enum>(uint32(a) & uint32(b));
}

class IRenderer
{
public:
	virtual ~IRenderer()
	{
	}

	virtual bool Init(const RendererDesc_t& rendererDesc) = 0;
	virtual void Shutdown() = 0;

	virtual void SetEnvironment() = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void Clear(
		uint32 clearFlags,
		const CColor& color = CColor::Black,
		float32 depth = 1.0f,
		uint32 stencil = 0) = 0;

	virtual void GetFramebufferSize(uint32& width, uint32& height) const = 0;
	virtual float32 GetAspectRatio() const = 0;
	virtual void SetFullViewport() = 0;

	virtual void SetViewport(const Viewport_t& viewport) = 0;
	virtual Viewport_t GetViewport() const = 0;
	virtual void SetScissor(const Rect_t& rect) = 0;
	virtual void SetStencilRef(uint32 stencilRef) = 0;

	virtual BufferHandle CreateVertexBuffer(
		const void* pData,
		uint64 size,
		BufferUsage_t::Enum usage) = 0;
	virtual BufferHandle CreateIndexBuffer(
		const void* pData,
		uint64 size,
		IndexFormat_t::Enum format,
		BufferUsage_t::Enum usage) = 0;
	virtual BufferHandle CreateConstantBuffer(
		uint64 size,
		BufferUsage_t::Enum usage) = 0;
	virtual void UpdateBuffer(
		BufferHandle hBuffer,
		const void* pData,
		uint64 size,
		uint64 offset = 0) = 0;
	virtual void DestroyBuffer(BufferHandle hBuffer) = 0;
	virtual void* MapBuffer(BufferHandle hBuffer) = 0;
	virtual void UnmapBuffer(BufferHandle hBuffer) = 0;
	virtual BufferHandle CreateStagingBuffer(uint64 size) = 0;

	virtual ShaderProgramHandle CreateShaderProgram(
		const CFixedString& shaderName) = 0;
	virtual ShaderProgramHandle GetOrCreateShaderProgram(
		const CFixedString& shaderName) = 0;
	virtual void DestroyShaderProgram(ShaderProgramHandle hProgram) = 0;
	virtual void ClearShaderCache() = 0;

	virtual TextureHandle CreateTexture2D(
		uint32 width,
		uint32 height,
		TextureFormat_t::Enum format,
		const void* pData = GCMGL_NULL) = 0;
	virtual TextureHandle CreateTextureCube(
		uint32 size,
		TextureFormat_t::Enum format,
		const void** ppFaces = GCMGL_NULL) = 0;
	virtual void SetTexture(
		TextureHandle hTexture,
		uint32 slot,
		ShaderStage_t stage) = 0;
	virtual void SetSampler(
		SamplerHandle hSampler,
		uint32 slot,
		ShaderStage_t stage) = 0;
	virtual void UpdateTexture(
		TextureHandle hTexture,
		const void* pData,
		uint32 mipLevel = 0) = 0;
	virtual void DestroyTexture(TextureHandle hTexture) = 0;

	virtual void SetShaderProgram(ShaderProgramHandle hProgram) = 0;
	virtual void SetVertexBuffer(
		BufferHandle hBuffer,
		uint32 slot = 0,
		uint32 vertexStride = 0,
		uint32 offset = 0,
		const CVertexLayout* pLayout = GCMGL_NULL) = 0;
	virtual void SetIndexBuffer(BufferHandle hBuffer, uint64 offset = 0) = 0;

	virtual UniformBlockLayoutHandle CreateUniformBlockLayout(
		const UniformBlockLayout_t& layout) = 0;
	virtual void SetConstantBuffer(
		BufferHandle hBuffer,
		UniformBlockLayoutHandle hLayout,
		uint32 slot,
		ShaderStage_t stage) = 0;
	virtual int32 GetUniformBlockBinding(
		ShaderProgramHandle hProgram,
		const char* pBlockName) = 0;

	virtual void SetBlendState(const BlendState_t& state) = 0;
	virtual void SetDepthStencilState(const DepthStencilState_t& state) = 0;

	virtual void ApplyVertexConstants(ShaderProgramHandle hProgram) = 0;
	virtual void ApplyFragmentConstants(ShaderProgramHandle hProgram) = 0;

	virtual void ExtractFrustumPlanes(
		const CMatrix4& mvp,
		Plane_t* pPlanes) = 0;
	virtual bool TestAABBFrustum(
		const CVector3& center,
		const CVector3& extent,
		const Plane_t* pPlanes) = 0;

	virtual void Draw(
		uint32 vertexCount,
		uint32 startVertex = 0,
		const CMatrix4* pViewProjection = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) = 0;
	virtual void DrawIndexed(
		uint32 indexCount,
		uint32 startIndex = 0,
		int32 baseVertex = 0,
		const CMatrix4* pViewProjection = GCMGL_NULL,
		const CVector3* pAABBCenter = GCMGL_NULL,
		const CVector3* pAABBExtent = GCMGL_NULL) = 0;
	virtual void DrawBatched(
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startVertex = 0) = 0;
	virtual void DrawIndexedBatched(
		uint32 indexCount,
		uint32 vertexCount,
		const CBatch& batch,
		const CMatrix4& viewProjection,
		uint32 startIndex = 0,
		int32 baseVertex = 0,
		bool isPerInstanceCull = true) = 0;
	virtual void DrawInstanced(
		uint32 vertexCount,
		uint32 instanceCount,
		const CMatrix4* pMatrices,
		const CVertexLayout* pInstanceLayout = GCMGL_NULL) = 0;
	virtual void DrawIndexedInstanced(
		uint32 indexCount,
		uint32 instanceCount,
		const CMatrix4* pMatrices,
		uint32 startIndex = 0,
		const CVertexLayout* pInstanceLayout = GCMGL_NULL) = 0;

	virtual void SetPipelineState(const PipelineState_t& state) = 0;
	virtual void FlushPipelineState() = 0;
};

class CRenderer : public virtual IRenderer
{
public:
	CRenderer();
	virtual ~CRenderer() GCMGL_OVERRIDE
	{
	}

	virtual BufferHandle CreateStagingBuffer(uint64 size) GCMGL_OVERRIDE;

	virtual ShaderProgramHandle GetOrCreateShaderProgram(
		const CFixedString& shaderName) GCMGL_OVERRIDE;
	virtual void ClearShaderCache() GCMGL_OVERRIDE;
	virtual float32 GetAspectRatio() const GCMGL_OVERRIDE;

	virtual void SetShaderProgram(ShaderProgramHandle hProgram) GCMGL_OVERRIDE;
	virtual void SetVertexBuffer(
		BufferHandle hBuffer,
		uint32 slot = 0,
		uint32 vertexStride = 0,
		uint32 offset = 0,
		const CVertexLayout* pLayout = GCMGL_NULL) GCMGL_OVERRIDE;
	virtual void SetIndexBuffer(
		BufferHandle hBuffer,
		uint64 offset = 0) GCMGL_OVERRIDE;

	virtual UniformBlockLayoutHandle CreateUniformBlockLayout(
		const UniformBlockLayout_t& layout) GCMGL_OVERRIDE;

	virtual void SetPipelineState(const PipelineState_t& state) GCMGL_OVERRIDE;
	virtual void FlushPipelineState() GCMGL_OVERRIDE;
protected:
	virtual void SetBlendState(const BlendState_t& state) = 0;
	virtual void SetDepthStencilState(const DepthStencilState_t& state) = 0;

	virtual void ExtractFrustumPlanes(
		const CMatrix4& mvp,
		Plane_t* pPlanes) GCMGL_OVERRIDE;
	virtual bool TestAABBFrustum(
		const CVector3& center,
		const CVector3& extent,
		const Plane_t* pPlanes) GCMGL_OVERRIDE;
	virtual FrustumVisibility_t::Enum GetAABBFrustumVisibility(
		const CVector3& center,
		const CVector3& extent,
		const Plane_t* pPlanes);

	virtual void BindVertexAttributes(
		const CVertexLayout* pLayout,
		uint32 vertexStride,
		uint32 offset) = 0;
	virtual void FlushProgramState() = 0;

	uint32 AllocHandle()
	{
		return m_NextHandle++;
	}

	PipelineState_t m_PipelineState;
	CUtlMap<CFixedString, ShaderProgramHandle> m_ShaderCache;
	CUtlMap<UniformBlockLayoutHandle, UniformBlockLayout_t> m_UniformBlockLayouts;
	StateDirtyFlags_t::Enum m_StateDirtyFlags;
	uint32 m_NextHandle;
};

IRenderer* CreateRenderer();
void DestroyRenderer(IRenderer* pRenderer);

#endif // RENDERER_H