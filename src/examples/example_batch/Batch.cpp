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

int32 RunBatchExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Init();

#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
	sysModuleLoad(SYSMODULE_NET);
	CNetPerfReporter::Init("192.168.8.195", 9000);
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3

	float64 startTime = CTime::GetTime();

	float32 rotationY = 0.0f;

	CCamera camera;
	camera.m_Position = CVector3(0.0f, 2.5f, 9.33f);

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram(
		"example_rainbow");
	if (hShaderProgram == 0)
	{
		Error("[Batch] Failed to create shader program 'example_rainbow'\n");

		return 1;
	}

	// Create cube
	Vertex_t vertices[] = {
		Vertex_t(
			CVector3(-0.5f, -0.5f, -0.5f),
			Vertex_t::PackColor(1.0f, 0.0f, 0.0f)), // Red
		Vertex_t(
			CVector3(0.5f, -0.5f, -0.5f),
			Vertex_t::PackColor(1.0f, 1.0f, 0.0f)), // Yellow
		Vertex_t(
			CVector3(0.5f, 0.5f, -0.5f),
			Vertex_t::PackColor(0.0f, 1.0f, 0.0f)), // Green
		Vertex_t(
			CVector3(-0.5f, 0.5f, -0.5f),
			Vertex_t::PackColor(0.0f, 0.0f, 1.0f)), // Blue

		Vertex_t(
			CVector3(-0.5f, -0.5f, 0.5f),
			Vertex_t::PackColor(1.0f, 0.0f, 1.0f)), // Magenta
		Vertex_t(
			CVector3(0.5f, -0.5f, 0.5f),
			Vertex_t::PackColor(1.0f, 1.0f, 1.0f)), // White
		Vertex_t(
			CVector3(0.5f, 0.5f, 0.5f),
			Vertex_t::PackColor(0.0f, 1.0f, 1.0f)), // Cyan
		Vertex_t(
			CVector3(-0.5f, 0.5f, 0.5f),
			Vertex_t::PackColor(0.0f, 0.0f, 0.0f)) // Black
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
		vertices,
		sizeof(vertices),
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

	// Create batch
	const int32 rows = 100;
	const int32 columns = 100;
	CUtlVector<CQuaternion> rotations;
	const float32 spacing = 3.0f;
	CBatch batch;

	for (int32 row = 0; row < rows; row++)
	{
		for (int32 col = 0; col < columns; col++)
		{
			float32 rotX = (row * 7.0f + col * 3.0f) * 5.0f;
			float32 rotY = (row * 11.0f + col * 13.0f) * 8.0f;
			float32 rotZ = (row * 17.0f + col * 19.0f) * 3.0f;
			CQuaternion rotation = CQuaternion::FromEuler(
				rotX * CMaths::Deg2Rad,
				rotY * CMaths::Deg2Rad,
				rotZ * CMaths::Deg2Rad);
			rotations.AddToTail(rotation);

			float32 posX = (col * spacing) - (columns - 1) * spacing * 0.5f;
			float32 posZ = (row * spacing) - (columns - 1) * spacing * 0.5f;
			batch.Add(
				CVector3(posX, 0.0f, posZ),
				rotation,
				CVector3(1.0f, 1.0f, 1.0f));
		}
	}
	batch.Build();

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
		pRenderer->SetFullViewport();

		// Set matrix
		camera.m_Position.m_X = sinf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f;
		camera.m_Position.m_Z =
			cosf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f + 9.33f;
		float32 aspectRatio = pRenderer->GetAspectRatio();
		CMatrix4 viewMatrix = camera.GetViewMatrix();
		CMatrix4 projectionMatrix = camera.GetProjectionMatrix(aspectRatio);
		CMatrix4 mvp = projectionMatrix * viewMatrix;

		// Update constant buffer
		pRenderer->UpdateBuffer(hConstantBuffer, &mvp, sizeof(CMatrix4), 0);

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
		
		// Animate cube positions and rotations based on time
		const CQuaternion rotation = CQuaternion::FromEuler(
			0.0f,
			rotationY * CMaths::Deg2Rad,
			0.0f);
		float32 time = float32(CTime::GetTime() - startTime);

		for (int32 chunkIndex = 0; chunkIndex < batch.m_BatchChunks.Count(); chunkIndex++)
		{
			BatchChunk_t& batchChunk = batch.m_BatchChunks[chunkIndex];

			const float32 batchChunkDistance = batchChunk.m_Center.Distance(
				camera.m_Position);
			if (!CBatchRenderer::ShouldUpdateChunk(
				batchChunkDistance,
				CTime::GetFrameCount()))
			{
				continue;
			}

			for (int32 j = 0; j < batchChunk.m_BatchChunkTransforms.Count(); j++)
			{
				BatchChunkTransform_t& batchChunkTransform =
					batchChunk.m_BatchChunkTransforms[j];
				const int32 transformIndex =
					chunkIndex * batchChunk.m_BatchChunkTransforms.Count() + j;
				batchChunkTransform.m_Rotation =
					rotation * rotations[transformIndex];

				const float32 chunkTransformDistance =
					batchChunkTransform.m_Position.Distance(camera.m_Position);
				const float32 distanceNorm = CMaths::Clamp(
					(chunkTransformDistance - 10.0f) / 490.0f,
					0.0f,
					1.0f);
				const float32 phase =
					batchChunkTransform.m_Position.m_X * 0.37f +
					batchChunkTransform.m_Position.m_Z * 0.73f;
				batchChunkTransform.m_Position.m_Y =
					(sinf((time + phase) * CMaths::TwoPI) * 0.5f + 0.5f) *
					distanceNorm * 20.0f;
			}
		}

		// Draw cubes
		batch.m_pCameraPos = &camera.m_Position;
		pRenderer->DrawIndexedBatched(
			36,
			8,
			batch,
			mvp,
			0);

		pRenderer->EndFrame();

#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
		CNetPerfReporter::Add("frame_us", PerfTimer_Now() - frameStartUs);
		CNetPerfReporter::Flush(float32(CTime::GetDeltaTime()));
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3
	}

	// Cleanup
#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
	CNetPerfReporter::Shutdown();
	sysModuleUnload(SYSMODULE_NET);
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}