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
#include "renderer/BatchRenderer.h"
#ifdef PLATFORM_PS3
#include "renderer/gcm/GcmRenderer.h"
#endif
#include "utils/Time.h"
#include "mathsfury/Maths.h"
#include "mathsfury/Quaternion.h"
#include "../Vertex.h"
#include "../Camera.h"
#include <stdio.h>
#include <math.h>

#ifdef GCMGL_DIAGNOSTICS
#include "utils/NetPerfReporter.h"
#include "utils/PerfTimer.h"
#endif // GCMGL_DIAGNOSTICS

#ifdef PLATFORM_PS3
#include <sysmodule/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#else // PLATFORM_PS3
#include <GLFW/glfw3.h>
#endif // !PLATFORM_PS3

int32 RunBatchInstancedExample(
	CWindowManager& windowManager,
#ifdef PLATFORM_PS3
	CGcmRenderer* pRenderer,
#else
	IRenderer* pRenderer,
#endif
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Initialize();

#ifdef GCMGL_DIAGNOSTICS
#ifdef PLATFORM_PS3
	sysModuleLoad(SYSMODULE_NET);
#endif // PLATFORM_PS3
	CNetPerfReporter::Init("192.168.8.195", 9000);
#endif // GCMGL_DIAGNOSTICS

	float64 startTime = CTime::GetTime();

	float32 rotationY = 0.0f;

	CCamera camera;
	camera.m_Position = CVector3(0.0f, 2.5f, 9.33f);

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram(
		"example_rainbow_instanced");
	if (hShaderProgram == 0)
	{
		Error(
			"[BatchInstanced] Failed to create shader 'example_rainbow_instanced'\n");

		return 1;
	}

	const int32 rows = 100;
	const int32 columns = 100;
	const float32 spacing = 3.0f;
	const int32 instanceCount = rows * columns;

	// Create cube
	Vertex_t vertexData[] = {
		Vertex_t(CVector3(-0.5f, -0.5f, -0.5f), Vertex_t::PackColor(1.0f, 0.0f, 0.0f)), // red
		Vertex_t(CVector3(0.5f, -0.5f, -0.5f), Vertex_t::PackColor(1.0f, 1.0f, 0.0f)), // yellow
		Vertex_t(CVector3(0.5f, 0.5f, -0.5f), Vertex_t::PackColor(0.0f, 1.0f, 0.0f)), // green
		Vertex_t(CVector3(-0.5f, 0.5f, -0.5f), Vertex_t::PackColor(0.0f, 0.0f, 1.0f)), // blue

		Vertex_t(CVector3(-0.5f, -0.5f, 0.5f), Vertex_t::PackColor(1.0f, 0.0f, 1.0f)), // magenta
		Vertex_t(CVector3(0.5f, -0.5f, 0.5f), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)), // white
		Vertex_t(CVector3(0.5f, 0.5f, 0.5f), Vertex_t::PackColor(0.0f, 1.0f, 1.0f)), // cyan
		Vertex_t(CVector3(-0.5f, 0.5f, 0.5f), Vertex_t::PackColor(0.0f, 0.0f, 0.0f)) // black
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

#ifdef PLATFORM_PS3
	BufferHandle hInstancedVertexBuffer = pRenderer->BuildInstancedVertexBuffer(
		hVertexBuffer,
		hIndexBuffer,
		36,
		uint32(instanceCount),
		sizeof(Vertex_t));
#else
	BufferHandle hInstancedVertexBuffer = hVertexBuffer;
#endif

	// Create uniform layout for view projection matrix
	UniformBlockLayout_t uniformLayout;
	uniformLayout.m_UniformNames.AddToTail("viewProjection");
	uniformLayout.m_Binding = 0;
	uniformLayout.m_Size = sizeof(CMatrix4);
	UniformBlockLayoutHandle hVPLayout = pRenderer->CreateUniformBlockLayout(
		uniformLayout);

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

	CVertexLayout instanceLayout;
	instanceLayout.AddAttribute(
		"modelCol0",
		uint32(VertexFormat_t::Float4),
		0,
		VertexSemantic_t::TexCoord1);
	instanceLayout.AddAttribute(
		"modelCol1",
		uint32(VertexFormat_t::Float4),
		16,
		VertexSemantic_t::TexCoord2);
	instanceLayout.AddAttribute(
		"modelCol2",
		uint32(VertexFormat_t::Float4),
		32,
		VertexSemantic_t::TexCoord3);
	instanceLayout.AddAttribute(
		"modelCol3",
		uint32(VertexFormat_t::Float4),
		48,
		VertexSemantic_t::TexCoord4);
	instanceLayout.SetStride(64);

	CUtlVector<CQuaternion> rotations;
	CUtlVector<CVector3> positions;
	rotations.SetCount(instanceCount);
	positions.SetCount(instanceCount);

	for (int32 row = 0; row < rows; row++)
	{
		for (int32 col = 0; col < columns; col++)
		{
			const int32 idx = row * columns + col;
			rotations[idx] = CQuaternion::FromEuler(
				(row * 7.0f + col * 3.0f) * 5.0f * CMaths::Deg2Rad,
				(row * 11.0f + col * 13.0f) * 8.0f * CMaths::Deg2Rad,
				(row * 17.0f + col * 19.0f) * 3.0f * CMaths::Deg2Rad);
			positions[idx] = CVector3(
				(col * spacing) - (columns - 1) * spacing * 0.5f,
				0.0f,
				(row * spacing) - (rows - 1) * spacing * 0.5f);
		}
	}

	CUtlVector<CMatrix4> matrices;
	matrices.SetCount(instanceCount);

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

#ifdef GCMGL_DIAGNOSTICS
		const uint64 frameStartUs = PerfTimer_Now();
#endif // GCMGL_DIAGNOSTICS

		rotationY += CTime::GetDeltaTime() * 30.0f;

		pRenderer->BeginFrame();

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f, 0);

		// Set viewport
		Viewport_t viewport(
			0.0f, 0.0f,
			float32(windowConfig.m_Width),
			float32(windowConfig.m_Height));
		pRenderer->SetViewport(viewport);

		// Set matrix
		camera.m_Position.m_X = sinf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f;
		camera.m_Position.m_Z =
			cosf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f + 9.33f;
		float32 aspectRatio = windowConfig.m_AspectRatio;
		CMatrix4 viewMatrix = camera.GetViewMatrix();
		CMatrix4 projectionMatrix = camera.GetProjectionMatrix(aspectRatio);
		CMatrix4 viewProjection = projectionMatrix * viewMatrix;

		// Animate cube positions and rotations based on time
		const CQuaternion rotation = CQuaternion::FromEuler(
			0.0f,
			rotationY * CMaths::Deg2Rad,
			0.0f);
		float32 time = static_cast<float32>(CTime::GetTime() - startTime);

		for (int32 i = 0; i < instanceCount; i++)
		{
			const float32 phase = positions[i].m_X * 0.37f + positions[i].m_Z * 0.73f;
			const float32 chunkTransformDistance = positions[i].Distance(
				camera.m_Position);
			const float32 distanceNorm = CMaths::Clamp(
				(chunkTransformDistance - 10.0f) / 490.0f,
				0.0f,
				1.0f);
			const float32 posY = (sinf((time + phase) * CMaths::TwoPI) * 0.5f + 0.5f) * distanceNorm * 20.0f;
			const CVector3 pos(positions[i].m_X, posY, positions[i].m_Z);
			const CQuaternion rot = rotation * rotations[i];
			matrices[i] = BatchChunkTransform_t(
				pos,
				rot,
				CVector3(1.0f, 1.0f, 1.0f)).ToMatrix();
		}

		// Update constant buffer
		pRenderer->UpdateBuffer(
			hConstantBuffer,
			&viewProjection,
			sizeof(CMatrix4),
			0);

		// Bind shader and buffers
		pRenderer->SetShaderProgram(hShaderProgram);

		pRenderer->SetConstantBuffer(
			hConstantBuffer,
			hVPLayout,
			0,
			ShaderStageVertex);
		pRenderer->SetVertexBuffer(
			hInstancedVertexBuffer,
			0,
			vertexLayout.GetStride(),
			0,
			&vertexLayout);
		pRenderer->SetIndexBuffer(hIndexBuffer);

#ifdef GCMGL_DIAGNOSTICS
		const uint64 drawStart = PerfTimer_Now();
#endif // GCMGL_DIAGNOSTICS

		pRenderer->DrawIndexedInstanced(
			36,
			uint32(instanceCount),
			matrices.Base(),
			0,
			&instanceLayout);

#ifdef GCMGL_DIAGNOSTICS
		CNetPerfReporter::Add("draw_us", PerfTimer_Now() - drawStart);
#endif // GCMGL_DIAGNOSTICS

		pRenderer->EndFrame();

#ifdef GCMGL_DIAGNOSTICS
		CNetPerfReporter::Add("frame_us", PerfTimer_Now() - frameStartUs);
		CNetPerfReporter::Flush(static_cast<float32>(CTime::GetDeltaTime()));
#endif // GCMGL_DIAGNOSTICS
	}

	// Cleanup
#ifdef GCMGL_DIAGNOSTICS
	CNetPerfReporter::Shutdown();
#ifdef PLATFORM_PS3
	sysModuleUnload(SYSMODULE_NET);
#endif // PLATFORM_PS3
#endif // GCMGL_DIAGNOSTICS
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	if (hInstancedVertexBuffer != hVertexBuffer)
		pRenderer->DestroyBuffer(hInstancedVertexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}
