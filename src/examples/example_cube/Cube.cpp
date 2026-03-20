//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Main.h"
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

int32 RunCubeExample(
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

	// Create cube
	Vertex_t vertexData[] = {
		// front
		{{-0.5f, -0.5f, -0.5f}, Vertex_t::PackColor(1.0f, 0.0f, 0.0f)}, // red
		{{0.5f, -0.5f, -0.5f}, Vertex_t::PackColor(1.0f, 1.0f, 0.0f)}, // yellow
		{{0.5f, 0.5f, -0.5f}, Vertex_t::PackColor(0.0f, 1.0f, 0.0f)}, // green
		{{-0.5f, 0.5f, -0.5f}, Vertex_t::PackColor(0.0f, 0.0f, 1.0f)}, // blue

		// back
		{{-0.5f, -0.5f, 0.5f}, Vertex_t::PackColor(1.0f, 0.0f, 1.0f)}, // magenta
		{{0.5f, -0.5f, 0.5f}, Vertex_t::PackColor(1.0f, 1.0f, 1.0f)}, // white
		{{0.5f, 0.5f, 0.5f}, Vertex_t::PackColor(0.0f, 1.0f, 1.0f)}, // cyan
		{{-0.5f, 0.5f, 0.5f}, Vertex_t::PackColor(0.0f, 0.0f, 0.0f)} // black
	};

	uint32 indexData[] = {
		0, 2, 1,
		0, 3, 2,

		1, 6, 5,
		1, 2, 6,

		5, 7, 4,
		5, 6, 7,

		4, 3, 0,
		4, 7, 3,

		3, 6, 2,
		3, 7, 6,

		4, 1, 5,
		4, 0, 1
	};

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
		"color", 
		uint32(VertexFormat_t::UByte4_Norm), 
		sizeof(CVector3),
		VertexSemantic_t::Color0);
	vertexLayout.SetStride(sizeof(Vertex_t));

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram(
		"rainbow");
	if (hShaderProgram == 0)
	{
		printf("[ERROR][Cube] Failed to create shader program 'rainbow'\n");

		return 1;
	}

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

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.15f, 1.0f), 1.0f, 0);

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
				CMatrix4(1.0f),
				rotationY,
				CVector3(0.0f, 1.0f, 0.0f)) *
			CMaths::Rotate(
				CMatrix4(1.0f),
				rotationX,
				CVector3(1.0f, 0.0f, 0.0f));
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

		// Draw cube
		pRenderer->DrawIndexed(36, 0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}