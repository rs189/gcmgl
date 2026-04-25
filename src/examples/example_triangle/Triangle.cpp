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
#else // PLATFORM_PS3
#include <GLFW/glfw3.h>
#endif // !PLATFORM_PS3

int32 RunTriangleExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Init();

	float32 rotation = 0.0f;

	CCamera camera;

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram(
		"example_rainbow");
	if (hShaderProgram == 0)
	{
		Error("[Triangle] Failed to create shader program 'example_rainbow'\n");

		return 1;
	}

	// Create triangle
	Vertex_t vertices[] = {
		Vertex_t(
			CVector3(-1.0f, -1.0f, 0.0f),
			Vertex_t::PackColor(1.0f, 0.0f, 0.0f)), // Red
		Vertex_t(
			CVector3(1.0f, -1.0f, 0.0f),
			Vertex_t::PackColor(0.0f, 1.0f, 0.0f)), // Green
		Vertex_t(
			CVector3(0.0f, 1.0f, 0.0f),
			Vertex_t::PackColor(0.0f, 0.0f, 1.0f)) // Blue
	};

	// Create buffers
	BufferHandle hVertexBuffer = pRenderer->CreateVertexBuffer(
		vertices,
		sizeof(vertices),
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

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

		rotation += CTime::GetDeltaTime() * 10.0f;

		pRenderer->BeginFrame();

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f, 0);

		// Set viewport
		pRenderer->SetFullViewport();

		// Set matrix
		camera.m_Position = CVector3(0.0f, 0.0f, 5.0f);
		float32 aspectRatio = pRenderer->GetAspectRatio();
		CMatrix4 projectionMatrix = camera.GetProjectionMatrix(aspectRatio);
		CMatrix4 viewMatrix = camera.GetViewMatrix();
		CMatrix4 model = CMaths::Rotate(
			CMatrix4(1.0f),
			rotation,
			CVector3(0.0f, 1.0f, 0.0f));
		CMatrix4 mvp = projectionMatrix * viewMatrix * model;

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

		// Draw triangle
		pRenderer->Draw(3, 0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}