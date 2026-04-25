//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Renderer.h"
#include "mathsfury/Maths.h"

CRenderer::CRenderer() :
	m_StateDirtyFlags(StateDirtyFlags_t::All),
	m_NextHandle(1)
{
	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_pVertexLayout = GCMGL_NULL;
	m_PipelineState.m_hShaderProgram = 0;
	m_PipelineState.m_hVertexBuffer = 0;
	m_PipelineState.m_hIndexBuffer = 0;
	m_PipelineState.m_VertexStride = 0;
	m_PipelineState.m_VertexOffset = 0;
	m_PipelineState.m_DepthStencilState.m_IsDepthTest = true;
	m_PipelineState.m_DepthStencilState.m_IsDepthWrite = true;
	m_PipelineState.m_BlendState.m_IsEnabled = false;
}

float32 CRenderer::GetAspectRatio() const
{
	uint32 width;
	uint32 height;
	GetFramebufferSize(width, height);
	if (height == 0) return 1.0f;

	return float32(width) / float32(height);
}

CVertexLayout::CVertexLayout() :
	m_VertexStride(0)
{
}

// Add a vertex attribute to the layout
void CVertexLayout::AddAttribute(
	const CFixedString& name,
	uint32 format,
	uint32 offset,
	uint32 location)
{
	const VertexAttribute_t attribute = {
		name,
		static_cast<VertexFormat_t::Enum>(format),
		offset,
		location,
		VertexSemantic_t::Unspecified
	};
	m_Attributes.AddToTail(attribute);
}

// Adds a vertex attribute with semantic to the layout
void CVertexLayout::AddAttribute(
	const CFixedString& name,
	uint32 format,
	uint32 offset,
	VertexSemantic_t::Enum vertexSemantic,
	uint32 location)
{
	const VertexAttribute_t attribute = {
		name,
		static_cast<VertexFormat_t::Enum>(format),
		offset,
		location,
		vertexSemantic
	};
	m_Attributes.AddToTail(attribute);
}

void CVertexLayout::SetStride(uint32 vertexStride)
{
	m_VertexStride = vertexStride;
}

bool PipelineState_t::operator==(const PipelineState_t& other) const
{
	return m_IndexOffset == other.m_IndexOffset &&
		m_pVertexLayout == other.m_pVertexLayout &&
		m_hShaderProgram == other.m_hShaderProgram &&
		m_hVertexBuffer == other.m_hVertexBuffer &&
		m_hIndexBuffer == other.m_hIndexBuffer &&
		m_VertexStride == other.m_VertexStride &&
		m_VertexOffset == other.m_VertexOffset &&
		m_DepthStencilState.m_IsDepthTest == other.m_DepthStencilState.m_IsDepthTest &&
		m_DepthStencilState.m_IsDepthWrite == other.m_DepthStencilState.m_IsDepthWrite &&
		m_BlendState.m_IsEnabled == other.m_BlendState.m_IsEnabled;
}

BufferHandle CRenderer::CreateStagingBuffer(uint64 size)
{
	return CreateConstantBuffer(size, BufferUsage_t::Dynamic);
}

ShaderProgramHandle CRenderer::GetOrCreateShaderProgram(
	const CFixedString& shaderName)
{
	CUtlMap<CFixedString, ShaderProgramHandle>::Index_t index = m_ShaderCache.Find(shaderName);
	if (index != m_ShaderCache.InvalidIndex())
	{
		return m_ShaderCache.Element(index);
	}

	ShaderProgramHandle hProgram = CreateShaderProgram(shaderName);
	if (hProgram != 0)
	{
		m_ShaderCache.Insert(shaderName, hProgram);
	}

	return hProgram;
}

void CRenderer::ClearShaderCache()
{
	for (CUtlMap<CFixedString, ShaderProgramHandle>::Index_t i = m_ShaderCache.FirstInorder();
		i != m_ShaderCache.InvalidIndex();
		i = m_ShaderCache.NextInorder(i))
	{
		DestroyShaderProgram(m_ShaderCache.Element(i));
	}
	m_ShaderCache.RemoveAll();
}

void CRenderer::SetShaderProgram(ShaderProgramHandle hProgram)
{
	if (m_PipelineState.m_hShaderProgram == hProgram)
	{
		return;
	}

	m_PipelineState.m_hShaderProgram = hProgram;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::Program | StateDirtyFlags_t::Uniforms;
}

void CRenderer::SetVertexBuffer(
	BufferHandle hBuffer,
	uint32 slot,
	uint32 vertexStride,
	uint32 offset,
	const CVertexLayout* pLayout)
{
	if (m_PipelineState.m_hVertexBuffer != hBuffer || 
		m_PipelineState.m_pVertexLayout != pLayout || 
		m_PipelineState.m_VertexStride != vertexStride || 
		m_PipelineState.m_VertexOffset != offset)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;
	}

	m_PipelineState.m_hVertexBuffer = hBuffer;
	m_PipelineState.m_pVertexLayout = pLayout;
	m_PipelineState.m_VertexStride = vertexStride;
	m_PipelineState.m_VertexOffset = offset;
}

void CRenderer::SetIndexBuffer(BufferHandle hBuffer, uint64 offset)
{
	if (m_PipelineState.m_hIndexBuffer != hBuffer || m_PipelineState.m_IndexOffset != offset)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::IndexBuffer;
	}

	m_PipelineState.m_hIndexBuffer = hBuffer;
	m_PipelineState.m_IndexOffset = offset;
}

UniformBlockLayoutHandle CRenderer::CreateUniformBlockLayout(
	const UniformBlockLayout_t& layout)
{
	uint32 handle = AllocHandle();
	m_UniformBlockLayouts.Insert(handle, layout);

	return handle;
}

void CRenderer::SetPipelineState(const PipelineState_t& state)
{
	if (m_PipelineState.m_IndexOffset != state.m_IndexOffset)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::IndexBuffer;
	}

	if (m_PipelineState.m_pVertexLayout != state.m_pVertexLayout ||
		m_PipelineState.m_VertexStride != state.m_VertexStride ||
		m_PipelineState.m_VertexOffset != state.m_VertexOffset)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;
	}

	if (m_PipelineState.m_hShaderProgram != state.m_hShaderProgram)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::Program | StateDirtyFlags_t::Uniforms;
	}

	if (m_PipelineState.m_hVertexBuffer != state.m_hVertexBuffer)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;
	}

	if (m_PipelineState.m_hIndexBuffer != state.m_hIndexBuffer)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::IndexBuffer;
	}

	if (m_PipelineState.m_DepthStencilState.m_IsDepthTest != state.m_DepthStencilState.m_IsDepthTest ||
		m_PipelineState.m_DepthStencilState.m_IsDepthWrite != state.m_DepthStencilState.m_IsDepthWrite)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::DepthStencilState;
	}

	if (m_PipelineState.m_BlendState.m_IsEnabled != state.m_BlendState.m_IsEnabled)
	{
		m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::BlendState;
	}

	m_PipelineState = state;
}

void CRenderer::FlushPipelineState()
{
	if ((m_StateDirtyFlags & StateDirtyFlags_t::Program) != StateDirtyFlags_t::None)
	{
		FlushProgramState();
	}

	if ((m_StateDirtyFlags & StateDirtyFlags_t::VertexBuffer) != StateDirtyFlags_t::None &&
		m_PipelineState.m_pVertexLayout != GCMGL_NULL)
	{
		BindVertexAttributes(
			m_PipelineState.m_pVertexLayout,
			m_PipelineState.m_VertexStride,
			m_PipelineState.m_VertexOffset);
	}

	if ((m_StateDirtyFlags & StateDirtyFlags_t::BlendState) != StateDirtyFlags_t::None)
	{
		SetBlendState(m_PipelineState.m_BlendState);
	}

	if ((m_StateDirtyFlags & StateDirtyFlags_t::DepthStencilState) != StateDirtyFlags_t::None)
	{
		SetDepthStencilState(m_PipelineState.m_DepthStencilState);
	}

	if ((m_StateDirtyFlags & StateDirtyFlags_t::Uniforms) != StateDirtyFlags_t::None)
	{
		ApplyVertexConstants(m_PipelineState.m_hShaderProgram);
		ApplyFragmentConstants(m_PipelineState.m_hShaderProgram);
	}

	m_StateDirtyFlags = StateDirtyFlags_t::None;
}

void CRenderer::ExtractFrustumPlanes(const CMatrix4& mvp, Plane_t* pPlanes)
{
	// Left
	pPlanes[0].m_Normal.m_X = mvp.m_Data[3] + mvp.m_Data[0];
	pPlanes[0].m_Normal.m_Y = mvp.m_Data[7] + mvp.m_Data[4];
	pPlanes[0].m_Normal.m_Z = mvp.m_Data[11] + mvp.m_Data[8];
	pPlanes[0].m_Distance = mvp.m_Data[15] + mvp.m_Data[12];

	// Right
	pPlanes[1].m_Normal.m_X = mvp.m_Data[3] - mvp.m_Data[0];
	pPlanes[1].m_Normal.m_Y = mvp.m_Data[7] - mvp.m_Data[4];
	pPlanes[1].m_Normal.m_Z = mvp.m_Data[11] - mvp.m_Data[8];
	pPlanes[1].m_Distance = mvp.m_Data[15] - mvp.m_Data[12];

	// Bottom
	pPlanes[2].m_Normal.m_X = mvp.m_Data[3] + mvp.m_Data[1];
	pPlanes[2].m_Normal.m_Y = mvp.m_Data[7] + mvp.m_Data[5];
	pPlanes[2].m_Normal.m_Z = mvp.m_Data[11] + mvp.m_Data[9];
	pPlanes[2].m_Distance = mvp.m_Data[15] + mvp.m_Data[13];

	// Top
	pPlanes[3].m_Normal.m_X = mvp.m_Data[3] - mvp.m_Data[1];
	pPlanes[3].m_Normal.m_Y = mvp.m_Data[7] - mvp.m_Data[5];
	pPlanes[3].m_Normal.m_Z = mvp.m_Data[11] - mvp.m_Data[9];
	pPlanes[3].m_Distance = mvp.m_Data[15] - mvp.m_Data[13];

	// Near
	pPlanes[4].m_Normal.m_X = mvp.m_Data[3] + mvp.m_Data[2];
	pPlanes[4].m_Normal.m_Y = mvp.m_Data[7] + mvp.m_Data[6];
	pPlanes[4].m_Normal.m_Z = mvp.m_Data[11] + mvp.m_Data[10];
	pPlanes[4].m_Distance = mvp.m_Data[15] + mvp.m_Data[14];

	// Far
	pPlanes[5].m_Normal.m_X = mvp.m_Data[3] - mvp.m_Data[2];
	pPlanes[5].m_Normal.m_Y = mvp.m_Data[7] - mvp.m_Data[6];
	pPlanes[5].m_Normal.m_Z = mvp.m_Data[11] - mvp.m_Data[10];
	pPlanes[5].m_Distance = mvp.m_Data[15] - mvp.m_Data[14];

	// Normalize
	for (int32 i = 0; i < 6; i++)
	{
		float32 len = pPlanes[i].m_Normal.Length();
		if (len > 0.0f)
		{
			float32 invLen = 1.0f / len;
			pPlanes[i].m_Normal.m_X *= invLen;
			pPlanes[i].m_Normal.m_Y *= invLen;
			pPlanes[i].m_Normal.m_Z *= invLen;
			pPlanes[i].m_Distance *= invLen;
		}
	}
}

bool CRenderer::TestAABBFrustum(
	const CVector3& center,
	const CVector3& extent,
	const Plane_t* pPlanes)
{
	for (int32 i = 0; i < 6; i++)
	{
		float32 r = extent.m_X * CMaths::Abs(pPlanes[i].m_Normal.m_X) +
					 extent.m_Y * CMaths::Abs(pPlanes[i].m_Normal.m_Y) +
					 extent.m_Z * CMaths::Abs(pPlanes[i].m_Normal.m_Z);

		// Distance from center to plane
		float32 distance = pPlanes[i].m_Normal.Dot(center) + pPlanes[i].m_Distance;

		// AABB is outside frustum
		if (distance + r < 0.0f)
		{
			return false;
		}
	}

	return true;
}

FrustumVisibility_t::Enum CRenderer::GetAABBFrustumVisibility(
	const CVector3& center,
	const CVector3& extent,
	const Plane_t* pPlanes)
{
	FrustumVisibility_t::Enum frustumVisibility = FrustumVisibility_t::Inside;
	for (int32 i = 0; i < 6; i++)
	{
		const float32 r =
			extent.m_X * CMaths::Abs(pPlanes[i].m_Normal.m_X) +
			extent.m_Y * CMaths::Abs(pPlanes[i].m_Normal.m_Y) +
			extent.m_Z * CMaths::Abs(pPlanes[i].m_Normal.m_Z);
		const float32 distance = pPlanes[i].m_Normal.Dot(center) + pPlanes[i].m_Distance;

		if (distance + r < 0.0f) return FrustumVisibility_t::Outside;
		if (distance - r < 0.0f) frustumVisibility = FrustumVisibility_t::Partial;
	}

	return frustumVisibility;
}