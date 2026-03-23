//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
#include "tier0/dbg.h"
#include "window/WindowManager.h"
#include "renderer/Renderer.h"
#include "utils/Time.h"
#include "mathsfury/Maths.h"
#include "../Vertex.h"
#include "../Camera.h"
#include <stdio.h>

#ifdef PLATFORM_PS3
#include <sysmodule/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#else
#include <GLFW/glfw3.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

struct TexturedVertex_t
{
	CVector3 m_Position;
	CVector2 m_TexCoord;
	uint32 m_Color;
};

int32 RunTexturedExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Initialize();

	float32 rotationX = 0.0f;
	float32 rotationY = 0.0f;

	CCamera camera;
	
	// Create shader
	ShaderProgramHandle hShaderProgram =
		pRenderer->CreateShaderProgram("example_textured");
	if (hShaderProgram == 0)
	{
		Error("[Textured] Failed to create shader program 'example_textured'\n");

		return 1;
	}

	// Create cube with per-face normals
	TexturedVertex_t vertexData[] = {
		// front (z = -0.5)
		{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},

		// back (z = 0.5)
		{{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},

		// right (x = 0.5)
		{{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},

		// left (x = -0.5)
		{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},

		// top (y = 0.5)
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},

		// bottom (y = -0.5)
		{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)},
		{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)}
	};

	uint32 indexData[] = {
		0, 2, 1,
		0, 3, 2,
		
		4, 6, 5,
		4, 7, 6,
 
		8, 10, 9,
		8, 11, 10,
 
		12, 14, 13,
		12, 15, 14,
 
		16, 18, 17,
		16, 19, 18,
 
		20, 22, 21,
		20, 23, 22
	};
 
	// Load texture from file
	int32 texWidth, texHeight, texChannels;
	CFixedString fullPath = "assets/texture.png";
#ifdef PLATFORM_PS3
	if (fullPath.find("/dev_hdd0/") != 0)
	{
		fullPath = "/dev_hdd0/game/GCGL00001/USRDIR/" + fullPath;
	}
#endif
	unsigned char* pRawImageData = stbi_load(
		fullPath.c_str(),
		&texWidth,
		&texHeight,
		&texChannels,
		4);
	uint32* pTextureData = GCMGL_NULL;
	int32 finalWidth = 512, finalHeight = 512;
 
	if (pRawImageData)
	{
		pTextureData = reinterpret_cast<uint32*>(pRawImageData); // RGBA
		finalWidth = texWidth;
		finalHeight = texHeight;
		Msg("[Textured] Loaded texture: %dx%d\n", texWidth, texHeight);
	}
	else
	{
		Error("[Textured] Failed to load texture.png\n");

		return 1;
	}
 
	// Create texture
	TextureHandle hTexture = pRenderer->CreateTexture2D(
		finalWidth,
		finalHeight,
		TextureFormat_t::RGBA8,
		pTextureData);
	stbi_image_free(pRawImageData);

	// Create buffers
	BufferHandle hVertexBuffer = pRenderer->CreateVertexBuffer(
		vertexData,
		sizeof(vertexData),
		BufferUsage_t::Static);
	BufferHandle hIndexBuffer = pRenderer->CreateIndexBuffer(
		indexData,
		sizeof(indexData),
		IndexFormat_t::UInt32,
		BufferUsage_t::Static);
	BufferHandle hConstantBuffer = pRenderer->CreateConstantBuffer(
		sizeof(CMatrix4),
		BufferUsage_t::Dynamic);

	// Create uniform layout for MVP matrix
	UniformBlockLayout_t uniformLayout;
	uniformLayout.m_UniformNames.AddToTail("mvp");
	uniformLayout.m_Binding = 0;
	uniformLayout.m_Size = sizeof(CMatrix4);
	uint32 hMVPLayout = pRenderer->CreateUniformBlockLayout(uniformLayout);

	// Vertex layout
	CVertexLayout vertexLayout;
	vertexLayout.AddAttribute(
		"position", 
		uint32(VertexFormat_t::Float3), 
		0,
		VertexSemantic_t::Position);
	vertexLayout.AddAttribute(
		"texCoord", 
		uint32(VertexFormat_t::Float2), 
		sizeof(CVector3),
		VertexSemantic_t::TexCoord0);
	vertexLayout.AddAttribute(
		"color", 
		uint32(VertexFormat_t::UByte4_Norm),
		sizeof(CVector3) + sizeof(CVector2),
		VertexSemantic_t::Color0);
	vertexLayout.SetStride(sizeof(TexturedVertex_t));

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

		rotationX += CTime::GetDeltaTime() * 30.0f;
		rotationY += CTime::GetDeltaTime() * 20.0f;
 
		pRenderer->BeginFrame();

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f, 0);

		// Set viewport
		Viewport_t viewport(
			0.0f, 0.0f,
			float32(windowConfig.m_Width),
			float32(windowConfig.m_Height));
		pRenderer->SetViewport(viewport);

		// Set matrix
		camera.m_Position = CVector3(0.0f, 0.0f, 5.0f);
		float32 aspect = windowConfig.m_AspectRatio;
		CMatrix4 proj = camera.GetProjectionMatrix(aspect);
		CMatrix4 view = camera.GetViewMatrix();

		CMatrix4 model =
			CMaths::Rotate(
				CMatrix4(1.0f), rotationY, CVector3(0.0f, 1.0f, 0.0f)) *
			CMaths::Rotate(
				CMatrix4(1.0f), rotationX, CVector3(1.0f, 0.0f, 0.0f));
		CMatrix4 mvp = proj * view * model;

		// Update constant buffer
		pRenderer->UpdateBuffer(hConstantBuffer, &mvp, sizeof(CMatrix4));

		// Bind shader and buffers
		pRenderer->SetShaderProgram(hShaderProgram);

		pRenderer->SetConstantBuffer(
			hConstantBuffer,
			hMVPLayout,
			0,
			ShaderStageVertex);
		pRenderer->SetVertexBuffer(
			hVertexBuffer,
			0,
			vertexLayout.GetStride(),
			0,
			&vertexLayout);
		pRenderer->SetIndexBuffer(hIndexBuffer);

		// Bind texture
		pRenderer->SetTexture(hTexture, 0, ShaderStageFragment);

		// Draw cube
		pRenderer->DrawIndexed(36, 0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyTexture(hTexture);
	pRenderer->DestroyShaderProgram(hShaderProgram);
 
	return 0;
}