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
#include <string.h>

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

static const int32 s_ChunkSize = 1000;
static const int32 s_MaxInstances = 50000;
static const int32 s_MaxChunks = (s_MaxInstances + s_ChunkSize - 1) / s_ChunkSize;

struct InstanceChunk_t
{
	CUtlVector<CMatrix4> m_Matrices;
	CVector3 m_AABBCenter;
	CVector3 m_AABBExtent;
	int32 m_InstanceCount;
};

int32 RunBatchInstancedExample(
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
	camera.m_Position = CVector3(0.0f, 5.0f, 9.33f);

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram(
		"example_rainbow_instanced");
	if (hShaderProgram == 0)
	{
		Error(
			"[BatchInstanced] Failed to create shader program 'example_rainbow_instanced'\n");

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

	// Create time buffer
	BufferHandle hTimeBuffer = pRenderer->CreateConstantBuffer(
		sizeof(float32) * 4,
		BufferUsage_t::Dynamic);

	// Create camera buffer
	BufferHandle hCameraBuffer = pRenderer->CreateConstantBuffer(
		sizeof(float32) * 4,
		BufferUsage_t::Dynamic);

	// Create uniform layout for view-projection matrix
	UniformBlockLayout_t viewProjectionLayout;
	viewProjectionLayout.m_UniformNames.AddToTail("viewProjection");
	viewProjectionLayout.m_Binding = 0;
	viewProjectionLayout.m_Size = sizeof(CMatrix4);
	UniformBlockLayoutHandle hViewProjectionLayout = pRenderer->CreateUniformBlockLayout(
		viewProjectionLayout);

	// Create uniform layout for time parameters
	UniformBlockLayout_t timeLayout;
	timeLayout.m_UniformNames.AddToTail("timeParams");
	timeLayout.m_Binding = 1;
	timeLayout.m_Size = sizeof(float32) * 4;
	UniformBlockLayoutHandle hTimeLayout = pRenderer->CreateUniformBlockLayout(
		timeLayout);

	// Create uniform layout for camera parameters
	UniformBlockLayout_t cameraLayout;
	cameraLayout.m_UniformNames.AddToTail("cameraParams");
	cameraLayout.m_Binding = 2;
	cameraLayout.m_Size = sizeof(float32) * 4;
	UniformBlockLayoutHandle hCameraLayout = pRenderer->CreateUniformBlockLayout(
		cameraLayout);

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

	// Instance layout
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

	// Create instances
	const int32 gridSize = int32(CMaths::Sqrt(float32(s_MaxInstances)));
	const float32 spacing = 3.0f;

	CUtlVector<CMatrix4> allMatrices;
	allMatrices.SetCount(s_MaxInstances);

	for (int32 i = 0; i < s_MaxInstances; i++)
	{
		const int32 row = i / gridSize;
		const int32 col = i % gridSize;
		const float32 posX = (col * spacing) - (gridSize - 1) * spacing * 0.5f;
		const float32 posZ = (row * spacing) - (gridSize - 1) * spacing * 0.5f;
		const float32 phase = (posX * 0.37f + posZ * 0.73f) * CMaths::TwoPI;
		const CQuaternion q = CQuaternion::FromEuler(
			(row * 7.0f + col * 3.0f) * 5.0f * CMaths::Deg2Rad,
			(row * 11.0f + col * 13.0f) * 8.0f * CMaths::Deg2Rad,
			(row * 17.0f + col * 19.0f) * 3.0f * CMaths::Deg2Rad);
		float32* d = allMatrices[i].m_Data;
		memset(d, 0, 16 * sizeof(float32));
		d[0] = posX;
		d[4] = posZ;
		d[8] = sinf(phase);
		d[12] = cosf(phase);
		d[1] = q.m_X;
		d[5] = q.m_Y;
		d[9] = q.m_Z;
		d[13] = q.m_W;
		const float32 dist = CMaths::Sqrt(
			posX * posX + (posZ - 9.33f) * (posZ - 9.33f));
		d[2] = CMaths::Clamp((dist - 10.0f) / 490.0f, 0.0f, 1.0f);
	}

	static InstanceChunk_t chunks[s_MaxChunks];
	int32 activeChunks = 0;
	int32 chunkIndex = 0;

	while (chunkIndex < s_MaxInstances && activeChunks < s_MaxChunks)
	{
		const int32 chunkStart = chunkIndex;
		const int32 chunkCount = CMaths::Min(
			s_ChunkSize,
			s_MaxInstances - chunkStart);

		InstanceChunk_t& instanceChunk = chunks[activeChunks];
		instanceChunk.m_InstanceCount = chunkCount;
		instanceChunk.m_Matrices.SetCount(chunkCount);

		float32 minX = 1e9f;
		float32 maxX = -1e9f;
		float32 minZ = 1e9f;
		float32 maxZ = -1e9f;
		for (int32 i = 0; i < chunkCount; i++)
		{
			instanceChunk.m_Matrices[i] = allMatrices[chunkStart + i];
			const float32 px = allMatrices[chunkStart + i].m_Data[0];
			const float32 pz = allMatrices[chunkStart + i].m_Data[4];
			if (px < minX)
			{
				minX = px;
			}
			if (px > maxX)
			{
				maxX = px;
			}
			if (pz < minZ)
			{
				minZ = pz;
			}
			if (pz > maxZ)
			{
				maxZ = pz;
			}
		}
		instanceChunk.m_AABBCenter = CVector3(
			(minX + maxX) * 0.5f,
			10.0f,
			(minZ + maxZ) * 0.5f);
		instanceChunk.m_AABBExtent = CVector3(
			(maxX - minX) * 0.5f + 0.5f,
			10.5f,
			(maxZ - minZ) * 0.5f + 0.5f);

		activeChunks++;
		chunkIndex += chunkCount;
	}

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
		const uint64 frameStartUs = PerfTimer_Now();
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3

		rotationY += CTime::GetDeltaTime() * 30.0f;

		pRenderer->BeginFrame();

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f, 0);

		// Set viewport
		Viewport_t viewport(
			0.0f,
			0.0f,
			float32(windowConfig.m_Width),
			float32(windowConfig.m_Height));
		pRenderer->SetViewport(viewport);

		// Set matrix
		camera.m_Position.m_X = sinf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f;
		camera.m_Position.m_Z =
			cosf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f + 9.33f;
		CMatrix4 viewProjection = camera.GetProjectionMatrix(
			windowConfig.m_AspectRatio) * camera.GetViewMatrix();

		// Update uniforms
		float32 time = float32(CTime::GetTime() - startTime);
		const float32 rotYRad = rotationY * CMaths::Deg2Rad;
		const float32 timeParams[4] = {
			sinf(time * CMaths::TwoPI),
			cosf(time * CMaths::TwoPI),
			sinf(rotYRad * 0.5f),
			cosf(rotYRad * 0.5f)
		};
		const float32 cameraParams[4] = {
			camera.m_Position.m_X,
			camera.m_Position.m_Z,
			0.0f,
			0.0f
		};

		pRenderer->UpdateBuffer(
			hConstantBuffer,
			&viewProjection,
			sizeof(CMatrix4),
			0);
		pRenderer->UpdateBuffer(
			hTimeBuffer,
			timeParams,
			sizeof(float32) * 4,
			0);
		pRenderer->UpdateBuffer(
			hCameraBuffer,
			cameraParams,
			sizeof(float32) * 4,
			0);

		// Bind shader and buffers
		pRenderer->SetShaderProgram(hShaderProgram);

		pRenderer->SetVertexBuffer(
			hVertexBuffer,
			0,
			vertexLayout.GetStride(),
			0,
			&vertexLayout);
		pRenderer->SetIndexBuffer(hIndexBuffer);

		pRenderer->SetConstantBuffer(
			hConstantBuffer,
			hViewProjectionLayout,
			0,
			ShaderStageVertex);
		pRenderer->SetConstantBuffer(
			hTimeBuffer,
			hTimeLayout,
			1,
			ShaderStageVertex);
		pRenderer->SetConstantBuffer(
			hCameraBuffer,
			hCameraLayout,
			2,
			ShaderStageVertex);

		// Extract frustum planes for culling
		Plane_t frustumPlanes[6];
		pRenderer->ExtractFrustumPlanes(viewProjection, frustumPlanes);

		int32 visibleChunks = 0;
#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
		const uint64 drawStart = PerfTimer_Now();
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3

		// Draw instances
		for (int32 chunkIndex = 0; chunkIndex < activeChunks; chunkIndex++)
		{
			InstanceChunk_t& instanceChunk = chunks[chunkIndex];
			if (!pRenderer->TestAABBFrustum(
				instanceChunk.m_AABBCenter,
				instanceChunk.m_AABBExtent,
				frustumPlanes))
			{
				continue;
			}

			visibleChunks++;
			pRenderer->DrawIndexedInstanced(
				36,
				uint32(instanceChunk.m_InstanceCount),
				instanceChunk.m_Matrices.Base(),
				0,
				&instanceLayout);
		}

		pRenderer->EndFrame();

#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
		CNetPerfReporter::Add("draw_us", PerfTimer_Now() - drawStart);
		CNetPerfReporter::Add("chunks", float32(visibleChunks));
		CNetPerfReporter::Add("instances", float32(activeChunks * s_ChunkSize));
		CNetPerfReporter::Add("frame_us", PerfTimer_Now() - frameStartUs);
		CNetPerfReporter::Flush(CTime::GetDeltaTime());
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3
	}

#ifdef PLATFORM_PS3
#ifdef GCMGL_DIAGNOSTICS
	CNetPerfReporter::Shutdown();
	sysModuleUnload(SYSMODULE_NET);
#endif // GCMGL_DIAGNOSTICS
#endif // PLATFORM_PS3

	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyBuffer(hTimeBuffer);
	pRenderer->DestroyBuffer(hCameraBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}