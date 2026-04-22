//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GlRenderer.h"
#include "GlBatchRenderer.h"
#include "tier0/dbg.h"
#include "utils/Utils.h"
#include "utils/UtlMemory.h"
#include "mathsfury/Maths.h"
#include "mathsfury/Vector4.h"
#include <string.h>
#include <GLFW/glfw3.h>

#ifdef SIMD_ENABLED
#include <simde/x86/sse.h>
#endif

CGlRenderer::CGlRenderer() :
	m_StagingIndex(0),
	m_InstanceBufferIndex(0),
	m_pWindow(GCMGL_NULL)
{
	for (int32 i = 0; i < s_MaxInstanceStagingBuffers; i++)
	{
		m_InstanceVertexBuffers[i] = 0;
		m_InstanceVertexBufferSizes[i] = 0;
	}
}

CGlRenderer::~CGlRenderer()
{
	Shutdown();
}

bool CGlRenderer::Init(const RendererDesc_t& rendererDesc)
{
	if (rendererDesc.m_pWindow != GCMGL_NULL)
	{
		m_pWindow = reinterpret_cast<GLFWwindow*>(rendererDesc.m_pWindow);
	}

	if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))
	{
		Error("[GLRenderer] Failed to initialize GLAD\n");

		return false;
	}

	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
	{
		Warning("[GLRenderer] OpenGL error during initialisation: %d\n", err);

		return false;
	}

	if (m_pWindow != GCMGL_NULL)
	{
		glfwMakeContextCurrent(m_pWindow);
		glfwSwapInterval(rendererDesc.m_IsVSync ? 1 : 0);
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	GLuint defaultVAO = 0;
	glGenVertexArrays(1, &defaultVAO);
	glBindVertexArray(defaultVAO);

	m_Viewport.m_X = 0.0f;
	m_Viewport.m_Y = 0.0f;
	m_Viewport.m_Width = float32(rendererDesc.m_Width);
	m_Viewport.m_Height = float32(rendererDesc.m_Height);

	m_ViewportScale[0] = m_Viewport.m_Width * 0.5f;
	m_ViewportScale[1] = m_Viewport.m_Height * -0.5f;
	m_ViewportScale[2] = 0.5f;
	m_ViewportScale[3] = 0.0f;
	m_ViewportOffset[0] = m_Viewport.m_X + m_Viewport.m_Width * 0.5f;
	m_ViewportOffset[1] = m_Viewport.m_Y + m_Viewport.m_Height * 0.5f;
	m_ViewportOffset[2] = 0.5f;
	m_ViewportOffset[3] = 0.0f;

	glViewport(
		static_cast<GLint>(m_Viewport.m_X),
		static_cast<GLint>(m_Viewport.m_Y),
		static_cast<GLsizei>(m_Viewport.m_Width),
		static_cast<GLsizei>(m_Viewport.m_Height));

	glScissor(
		static_cast<GLint>(m_Viewport.m_X),
		static_cast<GLint>(m_Viewport.m_Y),
		static_cast<GLsizei>(m_Viewport.m_Width),
		static_cast<GLsizei>(m_Viewport.m_Height));

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glShadeModel(GL_SMOOTH);
	glDepthMask(GL_TRUE);
	glFrontFace(GL_CCW);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDepthRange(0.0, 1.0);

	for (int32 i = 0; i < 8; i++)
	{
		glViewport(
			0,
			0,
			static_cast<GLsizei>(rendererDesc.m_Width),
			static_cast<GLsizei>(rendererDesc.m_Height));
	}

	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_CLIP_DISTANCE1);
	glDisable(GL_CLIP_DISTANCE2);
	glDisable(GL_CLIP_DISTANCE3);
	glDisable(GL_CLIP_DISTANCE4);
	glDisable(GL_CLIP_DISTANCE5);

	SetEnvironment();

	return true;
}

void CGlRenderer::Shutdown()
{
	for (uint32 i = 0; i < 2; i++)
	{
		if (m_StagingVertexBuffer[i].m_hId)
		{
			glDeleteBuffers(1, &m_StagingVertexBuffer[i].m_hId);
			m_StagingVertexBuffer[i].m_hId = 0;
		}

		if (m_StagingIndexBuffer[i].m_hId)
		{
			glDeleteBuffers(1, &m_StagingIndexBuffer[i].m_hId);
			m_StagingIndexBuffer[i].m_hId = 0;
		}

		m_StagingVertexBuffer[i].m_Data.RemoveAll();
		m_StagingIndexBuffer[i].m_Data.RemoveAll();

		if (m_StagingVertexBuffer[i].m_hBuffer)
		{
			m_BufferResources.Remove(m_StagingVertexBuffer[i].m_hBuffer);
			m_StagingVertexBuffer[i].m_hBuffer = 0;
		}

		if (m_StagingIndexBuffer[i].m_hBuffer)
		{
			m_BufferResources.Remove(m_StagingIndexBuffer[i].m_hBuffer);
			m_StagingIndexBuffer[i].m_hBuffer = 0;
		}
	}

	for (int32 i = m_TextureResources.FirstInorder(); m_TextureResources.IsValidIndex(i); i = m_TextureResources.NextInorder(i))
	{
		TextureResource_t& textureResource = m_TextureResources.Element(i);
		if (textureResource.m_hId)
		{
			GLuint id = textureResource.m_hId;
			glDeleteTextures(1, &id);
		}
	}
	m_TextureResources.RemoveAll();

	for (int32 i = m_ProgramResources.FirstInorder(); m_ProgramResources.IsValidIndex(i); i = m_ProgramResources.NextInorder(i))
	{
		ProgramResource_t& programResource = m_ProgramResources.Element(i);
		if (programResource.m_hId)
		{
			glDeleteProgram(programResource.m_hId);
		}
	}
	m_ProgramResources.RemoveAll();

	m_ProgramUniformShadows.RemoveAll();
	m_ProgramUniformBuffers.RemoveAll();

	for (int32 i = m_BufferResources.FirstInorder(); m_BufferResources.IsValidIndex(i); i = m_BufferResources.NextInorder(i))
	{
		BufferResource_t& bufferResource = m_BufferResources.Element(i);

		if (bufferResource.m_hId)
		{
			GLuint id = bufferResource.m_hId;
			glDeleteBuffers(1, &id);
			bufferResource.m_hId = 0;
		}

		if (bufferResource.m_pPtr)
		{
			if (bufferResource.m_IsAligned)
			{
				CUtlMemory::AlignedFree(bufferResource.m_pPtr);
			}
			else
			{
				CUtlMemory::Free(bufferResource.m_pPtr);
			}

			bufferResource.m_pPtr = GCMGL_NULL;
		}
	}
	m_BufferResources.RemoveAll();

	for (int32 i = 0; i < s_MaxInstanceStagingBuffers; i++)
	{
		if (m_InstanceVertexBuffers[i])
		{
			glDeleteBuffers(1, &m_InstanceVertexBuffers[i]);
			m_InstanceVertexBuffers[i] = 0;
			m_InstanceVertexBufferSizes[i] = 0;
		}
	}

	ClearShaderCache();
}

void CGlRenderer::SetEnvironment()
{
	glDepthMask(GL_TRUE);
}

void CGlRenderer::BeginFrame()
{
	m_StateDirtyFlags = StateDirtyFlags_t::All;
	m_InstanceBufferIndex = 0;
	m_StagingIndex = 0;

	SetEnvironment();
}

void CGlRenderer::EndFrame()
{
	if (m_pWindow)
	{
		glfwSwapBuffers(reinterpret_cast<GLFWwindow*>(m_pWindow));
	}
}

void CGlRenderer::Clear(
	uint32 clearFlags,
	const CColor& color,
	float32 depth,
	uint32 stencil)
{
	GLbitfield clearMask = 0;
	if (clearFlags & ClearColor)
	{
		glClearColor(color.m_R, color.m_G, color.m_B, color.m_A);
		clearMask |= GL_COLOR_BUFFER_BIT;
	}

	if (clearFlags & ClearDepth)
	{
		glClearDepth(depth);
		clearMask |= GL_DEPTH_BUFFER_BIT;
	}

	if (clearFlags & ClearStencil)
	{
		glClearStencil(static_cast<GLint>(stencil));
		clearMask |= GL_STENCIL_BUFFER_BIT;
	}

	glClear(clearMask);
}

void CGlRenderer::SetViewport(const Viewport_t& viewport)
{
	m_Viewport = viewport;

	glViewport(
		static_cast<GLint>(viewport.m_X),
		static_cast<GLint>(viewport.m_Y),
		static_cast<GLsizei>(viewport.m_Width),
		static_cast<GLsizei>(viewport.m_Height));
}

void CGlRenderer::SetScissor(const Rect_t& rect)
{
	glScissor(
		rect.m_X,
		rect.m_Y,
		static_cast<GLsizei>(rect.m_Width),
		static_cast<GLsizei>(rect.m_Height));
}

void CGlRenderer::SetStencilRef(uint32 stencilRef)
{
	glStencilFunc(GL_ALWAYS, static_cast<GLint>(stencilRef), 0xFF);
}

BufferHandle CGlRenderer::CreateVertexBuffer(
	const void* pData,
	uint64 size,
	BufferUsage_t::Enum usage)
{
	void* pPtr = CUtlMemory::AlignedAlloc(size, 16);
	if (!pPtr)
	{
		Error("[GLRenderer] Failed to allocate vertex buffer memory\n");

		return 0;
	}

	if (pData)
	{
		memcpy(pPtr, pData, static_cast<size_t>(size));
	}
	else
	{
		memset(pPtr, 0, static_cast<size_t>(size));
	}

	uint32 id;
	glGenBuffers(1, &id);
	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(size),
		pPtr,
		usage == BufferUsage_t::Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

	const BufferHandle hBuffer = AllocHandle();
	const BufferResource_t bufferResource = {
		size,
		pPtr,
		id,
		GL_ARRAY_BUFFER,
		true
	};
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGlRenderer::CreateIndexBuffer(
	const void* pData,
	uint64 size,
	IndexFormat_t::Enum format,
	BufferUsage_t::Enum usage)
{
	void* pPtr = CUtlMemory::AlignedAlloc(size, 16);
	if (!pPtr)
	{
		Error("[GLRenderer] Failed to allocate index buffer memory\n");

		return 0;
	}

	if (pData)
	{
		memcpy(pPtr, pData, static_cast<size_t>(size));
	}
	else
	{
		memset(pPtr, 0, static_cast<size_t>(size));
	}

	uint32 id;
	glGenBuffers(1, &id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(size),
		pPtr,
		usage == BufferUsage_t::Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

	const BufferHandle hBuffer = AllocHandle();
	const BufferResource_t bufferResource = {
		size,
		pPtr,
		id,
		GL_ELEMENT_ARRAY_BUFFER,
		true
	};
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

BufferHandle CGlRenderer::CreateConstantBuffer(uint64 size, BufferUsage_t::Enum usage)
{
	void* pPtr = CUtlMemory::Alloc(size);
	if (!pPtr)
	{
		Error("[GLRenderer] Failed to allocate constant buffer memory\n");

		return 0;
	}

	memset(pPtr, 0, static_cast<size_t>(size));

	const BufferHandle hBuffer = AllocHandle();
	const BufferResource_t bufferResource = {
		size,
		pPtr,
		0,
		0,
		false
	};
	m_BufferResources.Insert(hBuffer, bufferResource);

	return hBuffer;
}

void CGlRenderer::UpdateBuffer(
	BufferHandle hBuffer,
	const void* pData,
	uint64 size,
	uint64 offset)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	BufferResource_t& bufferResource = m_BufferResources.Element(bufferIndex);
	void* pDst = reinterpret_cast<uint8*>(bufferResource.m_pPtr) + offset;
	memcpy(pDst, pData, static_cast<size_t>(size));

	if (bufferResource.m_hId)
	{
		glBindBuffer(bufferResource.m_Target, bufferResource.m_hId);
		glBufferSubData(
			bufferResource.m_Target,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size),
			pData);
	}
}

void CGlRenderer::DestroyBuffer(BufferHandle hBuffer)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	BufferResource_t& bufferResource = m_BufferResources.Element(bufferIndex);
	if (bufferResource.m_hId)
	{
		glDeleteBuffers(1, &bufferResource.m_hId);
	}

	if (bufferResource.m_pPtr)
	{
		if (bufferResource.m_IsAligned)
		{
			CUtlMemory::AlignedFree(bufferResource.m_pPtr);
		}
		else
		{
			CUtlMemory::Free(bufferResource.m_pPtr);
		}
	}

	m_BufferResources.RemoveAt(bufferIndex);
}

void* CGlRenderer::MapBuffer(BufferHandle hBuffer)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid buffer handle: %d\n", hBuffer);

		return GCMGL_NULL;
	}

	return m_BufferResources.Element(bufferIndex).m_pPtr;
}

void CGlRenderer::UnmapBuffer(BufferHandle hBuffer)
{
	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	BufferResource_t& bufferResource = m_BufferResources.Element(bufferIndex);
	if (bufferResource.m_pPtr && bufferResource.m_hId)
	{
		glBindBuffer(bufferResource.m_Target, bufferResource.m_hId);
		glBufferData(
			bufferResource.m_Target,
			static_cast<GLsizeiptr>(bufferResource.m_Size),
			bufferResource.m_pPtr,
			GL_STATIC_DRAW);
	}
}

static uint32 CompileShaderFromSource(
	GLenum shaderType,
	const CFixedString& source)
{
	const char* pShaderSource = source.AsCharPtr();
	GLuint hShader = glCreateShader(shaderType);
	glShaderSource(hShader, 1, &pShaderSource, GCMGL_NULL);
	glCompileShader(hShader);

	GLint compileStatus;
	glGetShaderiv(hShader, GL_COMPILE_STATUS, &compileStatus);
	if (!compileStatus)
	{
		char buf[1024];
		glGetShaderInfoLog(hShader, sizeof(buf), GCMGL_NULL, buf);
		Error("[GLRenderer] Failed to compile shader: %s\n", buf);
		glDeleteShader(hShader);

		return 0;
	}

	return hShader;
}

ShaderProgramHandle CGlRenderer::CreateShaderProgram(
	const CFixedString& shaderName)
{
	CFixedString vertexProgramPath = CFixedString("shaders/glsl/") + shaderName + CFixedString(".vert");
	CFixedString vertexProgramSource = CUtils::ReadFile(
		vertexProgramPath.AsCharPtr());
	if (vertexProgramSource.IsEmpty())
	{
		Error(
			"[GLRenderer] Failed to read vertex shader source: %s\n",
			vertexProgramPath.AsCharPtr());

		return 0;
	}

	CFixedString fragmentProgramPath = CFixedString("shaders/glsl/") + shaderName + CFixedString(".frag");
	CFixedString fragmentProgramSource = CUtils::ReadFile(
		fragmentProgramPath.AsCharPtr());
	if (fragmentProgramSource.IsEmpty())
	{
		Error(
			"[GLRenderer] Failed to read fragment shader source: %s\n",
			fragmentProgramPath.AsCharPtr());

		return 0;
	}

	uint32 vertexShader = CompileShaderFromSource(
		GL_VERTEX_SHADER,
		vertexProgramSource);
	if (!vertexShader)
	{
		return 0;
	}

	uint32 fragmentShader = CompileShaderFromSource(
		GL_FRAGMENT_SHADER,
		fragmentProgramSource);
	if (!fragmentShader)
	{
		glDeleteShader(vertexShader);

		return 0;
	}

	GLuint glProgram = glCreateProgram();
	glAttachShader(glProgram, vertexShader);
	glAttachShader(glProgram, fragmentShader);
	glLinkProgram(glProgram);

	GLint linkStatus;
	glGetProgramiv(glProgram, GL_LINK_STATUS, &linkStatus);
	if (!linkStatus)
	{
		char buf[1024];
		glGetProgramInfoLog(glProgram, sizeof(buf), GCMGL_NULL, buf);
		Error("[GLRenderer] Failed to link shader program: %s\n", buf);
		glDeleteProgram(glProgram);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return 0;
	}

	glDetachShader(glProgram, vertexShader);
	glDeleteShader(vertexShader);
	glDetachShader(glProgram, fragmentShader);
	glDeleteShader(fragmentShader);

	const ShaderProgramHandle hProgram = AllocHandle();
	ProgramResource_t programResource;
	programResource.m_hId = glProgram;
	m_ProgramResources.Insert(hProgram, programResource);

	return hProgram;
}

void CGlRenderer::DestroyShaderProgram(ShaderProgramHandle hProgram)
{
	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	glDeleteProgram(m_ProgramResources.Element(programIndex).m_hId);

	m_ProgramResources.RemoveAt(programIndex);
}

TextureHandle CGlRenderer::CreateTexture2D(
	uint32 width,
	uint32 height,
	TextureFormat_t::Enum format,
	const void* pData)
{
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLenum internalFormat;
	GLenum glFormat;
	GLenum glType;

	switch (format)
	{
		case TextureFormat_t::R8:
			internalFormat = GL_R8;
			glFormat = GL_RED;
			glType = GL_UNSIGNED_BYTE;

			break;
		case TextureFormat_t::RGB8:
			internalFormat = GL_RGB8;
			glFormat = GL_RGB;
			glType = GL_UNSIGNED_BYTE;

			break;
		case TextureFormat_t::RGBA8:
			internalFormat = GL_RGBA8;
			glFormat = GL_RGBA;
			glType = GL_UNSIGNED_BYTE;

			break;
		default:
			internalFormat = GL_RGBA8;
			glFormat = GL_RGBA;
			glType = GL_UNSIGNED_BYTE;

			break;
	}

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		internalFormat,
		static_cast<GLsizei>(width),
		static_cast<GLsizei>(height),
		0,
		glFormat,
		glType,
		pData);

	const TextureHandle hTexture = AllocHandle();
	const TextureResource_t textureResource = {
		id,
		GL_TEXTURE_2D,
		width,
		height,
		format,
		false
	};
	m_TextureResources.Insert(hTexture, textureResource);

	return hTexture;
}

TextureHandle CGlRenderer::CreateTextureCube(
	uint32 size,
	TextureFormat_t::Enum format,
	const void** pFaces)
{
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, id);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	for (int32 i = 0; i < 6; i++)
	{
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + uint32(i),
			0,
			GL_RGBA8,
			static_cast<GLsizei>(size),
			static_cast<GLsizei>(size),
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pFaces ? pFaces[i] : GCMGL_NULL);
	}

	const TextureHandle hTexture = AllocHandle();
	const TextureResource_t textureResource = {
		id,
		GL_TEXTURE_CUBE_MAP,
		size,
		size,
		format,
		true
	};
	m_TextureResources.Insert(hTexture, textureResource);

	return hTexture;
}

void CGlRenderer::SetTexture(
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
		Warning("[GLRenderer] Invalid texture handle: %d\n", hTexture);

		return;
	}

	const TextureResource_t& textureResource = m_TextureResources.Element(
		textureIndex);

	glActiveTexture(GL_TEXTURE0 + slot);

	uint32 target = textureResource.m_Target ? textureResource.m_Target : GL_TEXTURE_2D;
	glBindTexture(target, textureResource.m_hId);
}

void CGlRenderer::SetSampler(
	SamplerHandle hSampler,
	uint32 slot,
	ShaderStage_t stage)
{
	Msg("[GLRenderer] SetSampler not implemented\n");
}

void CGlRenderer::UpdateTexture(
	TextureHandle hTexture,
	const void* pData,
	uint32 mipLevel)
{
	int32 textureIndex = m_TextureResources.Find(hTexture);
	if (textureIndex == m_TextureResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid texture handle: %d\n", hTexture);

		return;
	}

	glBindTexture(
		GL_TEXTURE_2D,
		m_TextureResources.Element(textureIndex).m_hId);
}

void CGlRenderer::DestroyTexture(TextureHandle hTexture)
{
	int32 textureIndex = m_TextureResources.Find(hTexture);
	if (textureIndex == m_TextureResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid texture handle: %d\n", hTexture);

		return;
	}

	uint32 id = m_TextureResources.Element(textureIndex).m_hId;
	if (id)
	{
		glDeleteTextures(1, &id);
	}

	m_TextureResources.RemoveAt(textureIndex);
}

void CGlRenderer::SetConstantBuffer(
	BufferHandle hBuffer,
	UniformBlockLayoutHandle hLayout,
	uint32 slot,
	ShaderStage_t stage)
{
	if (hBuffer == 0) return;

	int32 bufferIndex = m_BufferResources.Find(hBuffer);
	if (bufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid buffer handle: %d\n", hBuffer);

		return;
	}

	int32 layoutIndex = m_UniformBlockLayouts.Find(hLayout);
	if (layoutIndex == m_UniformBlockLayouts.InvalidIndex())
	{
		Warning("[GLRenderer] Invalid uniform layout handle: %d\n", hLayout);

		return;
	}

	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GLRenderer] Invalid shader program bound\n");

		return;
	}

	BoundUniform_t& boundUniform = m_ProgramUniformBuffers[m_PipelineState.m_hShaderProgram][hLayout];
	boundUniform.m_hBuffer = hBuffer;
	boundUniform.m_Slot = slot;
	boundUniform.m_Stage = stage;

	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::Uniforms;
}

void CGlRenderer::SetBlendState(const BlendState_t& state)
{
	if (state.m_IsEnabled)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

void CGlRenderer::SetDepthStencilState(const DepthStencilState_t& state)
{
	if (state.m_IsDepthTest)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}

	glDepthMask(state.m_IsDepthWrite ? GL_TRUE : GL_FALSE);
}

void CGlRenderer::ApplyVertexConstants(ShaderProgramHandle hProgram)
{
	if (hProgram == 0)
	{
		Warning("[GLRenderer] Invalid shader program handle: %d\n", hProgram);

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
		Warning("[GLRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	CUtlMap<uint32, BoundUniform_t>& uniforms = m_ProgramUniformBuffers.Element(
		uniformsIndex);
	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);

	for (int32 uniformIndex = uniforms.FirstInorder(); uniforms.IsValidIndex(uniformIndex); uniformIndex = uniforms.NextInorder(uniformIndex))
	{
		const BoundUniform_t& boundUniform = uniforms.Element(uniformIndex);
		if (boundUniform.m_Stage != ShaderStageVertex) continue;

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

		bool hasChanged = uniformShadow.m_IsDirty || memcmp(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size) != 0;
		if (!hasChanged) continue;

		memcpy(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size);

		if (uniformLayout.m_UniformNames.IsEmpty()) continue;

		CUtlMap<CFixedString, int32>& uniformLocationMap = programResource.m_UniformLocations;
		const uint8* pShadowData = uniformShadow.m_Data.Base();

		uint32 uniformBytes = uniformLayout.m_Size / uniformLayout.m_UniformNames.Count();

		for (int32 uniformNameIndex = 0; uniformNameIndex < uniformLayout.m_UniformNames.Count(); uniformNameIndex++)
		{
			const CFixedString& uniformName = uniformLayout.m_UniformNames[uniformNameIndex];

			int32 uniformLocation = -1;
			int32 uniformMapIndex = uniformLocationMap.Find(uniformName);
			if (uniformMapIndex != uniformLocationMap.InvalidIndex())
			{
				uniformLocation = uniformLocationMap.Element(uniformMapIndex);
			}
			else
			{
				uniformLocation = glGetUniformLocation(
					programResource.m_hId,
					uniformName.AsCharPtr());
				uniformLocationMap.Insert(uniformName, uniformLocation);
			}

			if (uniformLocation < 0) continue;

			const GLfloat* pElementData = reinterpret_cast<const GLfloat*>(
				pShadowData + (uint32(uniformNameIndex) * uniformBytes));

			if (uniformBytes == 64)
			{
				glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, pElementData);
			}
			else if (uniformBytes == 16)
			{
				glUniform4fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 12)
			{
				glUniform3fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 8)
			{
				glUniform2fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 4)
			{
				glUniform1fv(uniformLocation, 1, pElementData);
			}
		}

		uniformShadow.m_IsDirty = false;
	}
}

void CGlRenderer::ApplyFragmentConstants(ShaderProgramHandle hProgram)
{
	if (hProgram == 0)
	{
		Warning("[GLRenderer] Invalid shader program handle: %d\n", hProgram);

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
		Warning("[GLRenderer] Invalid shader program handle: %d\n", hProgram);

		return;
	}

	CUtlMap<uint32, BoundUniform_t>& uniforms = m_ProgramUniformBuffers.Element(
		uniformsIndex);
	ProgramResource_t& programResource = m_ProgramResources.Element(programIndex);

	for (int32 uniformIndex = uniforms.FirstInorder(); uniforms.IsValidIndex(uniformIndex); uniformIndex = uniforms.NextInorder(uniformIndex))
	{
		const BoundUniform_t& boundUniform = uniforms.Element(uniformIndex);
		if (boundUniform.m_Stage != ShaderStageFragment) continue;

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

		bool hasChanged = uniformShadow.m_IsDirty || memcmp(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size) != 0;
		if (!hasChanged) continue;

		memcpy(uniformShadow.m_Data.Base(), pBufferData, uniformLayout.m_Size);

		if (uniformLayout.m_UniformNames.IsEmpty()) continue;

		CUtlMap<CFixedString, int32>& uniformLocationMap = programResource.m_UniformLocations;
		const uint8* pShadowData = uniformShadow.m_Data.Base();

		uint32 uniformBytes = uniformLayout.m_Size / uniformLayout.m_UniformNames.Count();

		for (int32 uniformNameIndex = 0; uniformNameIndex < uniformLayout.m_UniformNames.Count(); uniformNameIndex++)
		{
			const CFixedString& uniformName = uniformLayout.m_UniformNames[uniformNameIndex];

			int32 uniformLocation = -1;
			int32 uniformMapIndex = uniformLocationMap.Find(uniformName);
			if (uniformMapIndex != uniformLocationMap.InvalidIndex())
			{
				uniformLocation = uniformLocationMap.Element(uniformMapIndex);
			}
			else
			{
				uniformLocation = glGetUniformLocation(
					programResource.m_hId,
					uniformName.AsCharPtr());
				uniformLocationMap.Insert(uniformName, uniformLocation);
			}

			if (uniformLocation < 0) continue;

			const GLfloat* pElementData = reinterpret_cast<const GLfloat*>(
				pShadowData + (uint32(uniformNameIndex) * uniformBytes));

			if (uniformBytes == 64)
			{
				glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, pElementData);
			}
			else if (uniformBytes == 16)
			{
				glUniform4fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 12)
			{
				glUniform3fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 8)
			{
				glUniform2fv(uniformLocation, 1, pElementData);
			}
			else if (uniformBytes == 4)
			{
				glUniform1fv(uniformLocation, 1, pElementData);
			}
		}

		uniformShadow.m_IsDirty = false;
	}
}

void CGlRenderer::Draw(
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

	FlushPipelineState();

	glDrawArrays(
		GL_TRIANGLES,
		static_cast<GLint>(startVertex),
		static_cast<GLsizei>(vertexCount));
}

void CGlRenderer::DrawIndexed(
	uint32 indexCount,
	uint32 startIndex,
	int32 baseVertex,
	const CMatrix4* pViewProjection,
	const CVector3* pAABBCenter,
	const CVector3* pAABBExtent)
{
	if (m_PipelineState.m_hIndexBuffer == 0)
	{
		Warning("[GLRenderer] Invalid index buffer bound\n");

		return;
	}

	if (m_PipelineState.m_hShaderProgram == 0)
	{
		Warning("[GLRenderer] Invalid shader program bound\n");

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

	uint64 indexOffset = m_PipelineState.m_IndexOffset + uint64(startIndex) * sizeof(uint32);
	int32 indexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hIndexBuffer);
	if (indexBufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning(
			"[GLRenderer] Invalid index buffer handle: %d\n",
			m_PipelineState.m_hIndexBuffer);

		return;
	}

	glBindBuffer(
		GL_ELEMENT_ARRAY_BUFFER,
		m_BufferResources.Element(indexBufferIndex).m_hId);

	FlushPipelineState();

	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(indexCount),
		GL_UNSIGNED_INT,
		reinterpret_cast<void*>(static_cast<uintptr_t>(indexOffset)));
}

void CGlRenderer::DrawInstanced(
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

	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);
	CUtlMap<CFixedString, int32>& attributeMap = programResource.m_AttributeLocations;

	FlushProgramState();

	ApplyVertexConstants(hProgram);
	ApplyFragmentConstants(hProgram);

	const CUtlVector<VertexAttribute_t>& attributes = pInstanceLayout->GetAttributes();

	uint64 uploadSize = uint64(instanceCount) * 16 * sizeof(float32);
	uint32& instanceVertexBuffer = m_InstanceVertexBuffers[m_InstanceBufferIndex];
	uint64& instanceVertexBufferSize = m_InstanceVertexBufferSizes[m_InstanceBufferIndex];

	if (instanceVertexBuffer == 0)
	{
		glGenBuffers(1, &instanceVertexBuffer);
	}

	glBindBuffer(GL_ARRAY_BUFFER, instanceVertexBuffer);

	if (uploadSize > instanceVertexBufferSize)
	{
		glBufferData(
			GL_ARRAY_BUFFER,
			uploadSize,
			pMatrices[0].m_Data,
			GL_DYNAMIC_DRAW);
		instanceVertexBufferSize = uploadSize;
	}
	else
	{
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			uploadSize,
			pMatrices[0].m_Data);
	}

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_Name.IsEmpty()) continue;

		int32 attributeMapIndex = attributeMap.Find(attribute.m_Name);
		int32 loc = (attributeMapIndex != attributeMap.InvalidIndex())
			? attributeMap.Element(attributeMapIndex)
			: glGetAttribLocation(
				programResource.m_hId,
				attribute.m_Name.AsCharPtr());
		if (attributeMapIndex == attributeMap.InvalidIndex())
		{
			attributeMap.Insert(attribute.m_Name, loc);
		}

		if (loc < 0) continue;
		glEnableVertexAttribArray(loc);
		glVertexAttribPointer(
			loc, 4, GL_FLOAT, GL_FALSE,
			int32(pInstanceLayout->GetStride()),
			reinterpret_cast<void*>(uint64(attribute.m_Offset)));
		glVertexAttribDivisor(loc, 1);
	}

	FlushPipelineState();

	glDrawArraysInstanced(
		GL_TRIANGLES,
		0,
		int32(vertexCount),
		int32(instanceCount));

	m_InstanceBufferIndex = (m_InstanceBufferIndex + 1) % s_MaxInstanceStagingBuffers;

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_Name.IsEmpty()) continue;
		int32 attributeMapIndex = attributeMap.Find(attribute.m_Name);
		if (attributeMapIndex != attributeMap.InvalidIndex())
		{
			int32 loc = attributeMap.Element(attributeMapIndex);
			if (loc >= 0)
			{
				glVertexAttribDivisor(loc, 0);
			}
		}
	}
}

void CGlRenderer::DrawIndexedInstanced(
	uint32 indexCount,
	uint32 instanceCount,
	const CMatrix4* pMatrices,
	uint32 startIndex,
	const CVertexLayout* pInstanceLayout)
{
	if (instanceCount == 0 || !pInstanceLayout) return;

	const ShaderProgramHandle hProgram = m_PipelineState.m_hShaderProgram;
	if (!hProgram) return;

	int32 programIndex = m_ProgramResources.Find(hProgram);
	if (programIndex == m_ProgramResources.InvalidIndex()) return;

	ProgramResource_t& programResource = m_ProgramResources.Element(
		programIndex);
	CUtlMap<CFixedString, int32>& attributeMap = programResource.m_AttributeLocations;

	FlushProgramState();

	ApplyVertexConstants(hProgram);
	ApplyFragmentConstants(hProgram);

	const CUtlVector<VertexAttribute_t>& attributes = pInstanceLayout->GetAttributes();

	const uint32 instanceStride = pInstanceLayout->GetStride();
	const uint64 uploadSize = uint64(instanceCount) * instanceStride;
	uint32& instanceVertexBuffer = m_InstanceVertexBuffers[m_InstanceBufferIndex];
	uint64& instanceVertexBufferSize = m_InstanceVertexBufferSizes[m_InstanceBufferIndex];

	CUtlVector<float32> transposedData;
	transposedData.SetCount(int32(instanceCount) * 16);
	for (uint32 i = 0; i < instanceCount; i++)
	{
		const CMatrix4& matrix = pMatrices[i];
		float32* p = transposedData.Base() + i * 16;
		for (int32 row = 0; row < 4; row++)
		{
			p[row * 4 + 0] = matrix.m_Data[row];
			p[row * 4 + 1] = matrix.m_Data[4 + row];
			p[row * 4 + 2] = matrix.m_Data[8 + row];
			p[row * 4 + 3] = matrix.m_Data[12 + row];
		}
	}

	if (instanceVertexBuffer == 0)
	{
		glGenBuffers(1, &instanceVertexBuffer);
	}

	glBindBuffer(GL_ARRAY_BUFFER, instanceVertexBuffer);

	if (uploadSize > instanceVertexBufferSize)
	{
		glBufferData(
			GL_ARRAY_BUFFER,
			uploadSize,
			transposedData.Base(),
			GL_DYNAMIC_DRAW);
		instanceVertexBufferSize = uploadSize;
	}
	else
	{
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			uploadSize,
			transposedData.Base());
	}

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_Name.IsEmpty()) continue;

		int32 attributeMapIndex = attributeMap.Find(attribute.m_Name);
		int32 loc = (attributeMapIndex != attributeMap.InvalidIndex())
			? attributeMap.Element(attributeMapIndex)
			: glGetAttribLocation(
				programResource.m_hId,
				attribute.m_Name.AsCharPtr());
		if (attributeMapIndex == attributeMap.InvalidIndex())
		{
			attributeMap.Insert(attribute.m_Name, loc);
		}

		if (loc < 0) continue;
		glEnableVertexAttribArray(loc);
		glVertexAttribPointer(
			loc, 4, GL_FLOAT, GL_FALSE,
			int32(instanceStride),
			reinterpret_cast<void*>(uint64(attribute.m_Offset)));
		glVertexAttribDivisor(loc, 1);
	}

	int32 indexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hIndexBuffer);
	if (indexBufferIndex == m_BufferResources.InvalidIndex()) return;
	glBindBuffer(
		GL_ELEMENT_ARRAY_BUFFER,
		m_BufferResources.Element(indexBufferIndex).m_hId);

	FlushPipelineState();

	glDrawElementsInstanced(
		GL_TRIANGLES,
		int32(indexCount),
		GL_UNSIGNED_INT,
		reinterpret_cast<void*>(uint64(startIndex) * sizeof(uint32)),
		int32(instanceCount));

	m_InstanceBufferIndex = (m_InstanceBufferIndex + 1) % s_MaxInstanceStagingBuffers;

	for (int32 i = 0; i < attributes.Count(); i++)
	{
		const VertexAttribute_t& attribute = attributes[i];
		if (attribute.m_Name.IsEmpty()) continue;
		int32 attributeMapIndex = attributeMap.Find(attribute.m_Name);
		if (attributeMapIndex != attributeMap.InvalidIndex())
		{
			int32 loc = attributeMap.Element(attributeMapIndex);
			if (loc >= 0)
			{
				glVertexAttribDivisor(loc, 0);
			}
		}
	}
}

uint32 CGlRenderer::GetVertexSemanticAttributeIndex(
	VertexSemantic_t::Enum vertexSemantic)
{
	return 0xFFFFFFFFu;
}

void CGlRenderer::BindVertexAttributes(
	const CVertexLayout* pLayout,
	uint32 vertexStride,
	uint32 offset)
{
	if (!pLayout)
	{
		Warning("[GLRenderer] Invalid vertex layout\n");

		return;
	}

	if (m_PipelineState.m_hVertexBuffer == 0)
	{
		Warning("[GLRenderer] Invalid vertex buffer bound\n");

		return;
	}

	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex())
	{
		Warning(
			"[GLRenderer] Invalid vertex buffer handle: %d\n",
			m_PipelineState.m_hVertexBuffer);

		return;
	}

	int32 programIndex = m_ProgramResources.Find(
		m_PipelineState.m_hShaderProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning(
			"[GLRenderer] Invalid shader program handle: %d\n",
			m_PipelineState.m_hShaderProgram);

		return;
	}

	glBindBuffer(
		GL_ARRAY_BUFFER,
		m_BufferResources.Element(vertexBufferIndex).m_hId);

	const CUtlVector<VertexAttribute_t>& attributes = pLayout->GetAttributes();
	ProgramResource_t& programResource = m_ProgramResources.Element(programIndex);

	for (int32 attributeIndex = 0; attributeIndex < attributes.Count(); attributeIndex++)
	{
		const VertexAttribute_t& attribute = attributes[attributeIndex];
		if (attribute.m_Name.IsEmpty()) continue;

		GLint loc = -1;
		if (attribute.m_VertexSemantic != VertexSemantic_t::Unspecified)
		{
			CUtlMap<CFixedString, int32>& attributeMap = programResource.m_AttributeLocations;
			int32 attributeMapIndex = attributeMap.Find(attribute.m_Name);
			if (attributeMapIndex != attributeMap.InvalidIndex())
			{
				loc = attributeMap.Element(attributeMapIndex);
			}
			else
			{
				loc = glGetAttribLocation(
					programResource.m_hId,
					attribute.m_Name.AsCharPtr());
				attributeMap.Insert(attribute.m_Name, loc);
			}
		}

		if (loc == -1) continue;

		glEnableVertexAttribArray(static_cast<GLuint>(loc));

		GLint components;
		GLenum dataType;
		GLboolean normalized;

		switch (attribute.m_Format)
		{
			case VertexFormat_t::Float:
				components = 1;
				dataType = GL_FLOAT;
				normalized = GL_FALSE;

				break;
			case VertexFormat_t::Float2:
				components = 2;
				dataType = GL_FLOAT;
				normalized = GL_FALSE;

				break;
			case VertexFormat_t::Float3:
				components = 3;
				dataType = GL_FLOAT;
				normalized = GL_FALSE;

				break;
			case VertexFormat_t::Float4:
				components = 4;
				dataType = GL_FLOAT;
				normalized = GL_FALSE;

				break;
			case VertexFormat_t::UByte4_Norm:
				components = 4;
				dataType = GL_UNSIGNED_BYTE;
				normalized = GL_TRUE;

				break;
			default:
				components = 3;
				dataType = GL_FLOAT;
				normalized = GL_FALSE;

				break;
		}

		uint32 attributeOffset = attribute.m_Offset + offset;
		glVertexAttribPointer(
			static_cast<GLuint>(loc),
			components,
			dataType,
			normalized,
			static_cast<GLsizei>(
				vertexStride ? vertexStride : pLayout->GetStride()),
			reinterpret_cast<const void*>(
				static_cast<uintptr_t>(attributeOffset)));
	}
}

void CGlRenderer::FlushProgramState()
{
	int32 programIndex = m_ProgramResources.Find(
		m_PipelineState.m_hShaderProgram);
	if (programIndex == m_ProgramResources.InvalidIndex())
	{
		Warning(
			"[GCMRenderer] Invalid shader program handle: %d\n",
			m_PipelineState.m_hShaderProgram);

		glUseProgram(0);

		return;
	}

	glUseProgram(m_ProgramResources.Element(programIndex).m_hId);
}

IRenderer* CreateRenderer()
{
	void* pRendererMemory = CUtlMemory::Alloc(sizeof(CGlBatchRenderer));
	if (pRendererMemory)
	{
		return new(pRendererMemory) CGlBatchRenderer();
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