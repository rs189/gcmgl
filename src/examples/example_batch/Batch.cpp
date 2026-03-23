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
#include "mathsfury/Quaternion.h"
#include "../Vertex.h"
#include "../Camera.h"
#include <stdio.h>
#include <math.h>

#ifdef PLATFORM_PS3
#include <sysmodule/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#else
#include <GLFW/glfw3.h>
#endif

int32 RunBatchExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Initialize();

	double startTime = CTime::GetTime();

	float32 rotationX = 0.0f;
	float32 rotationY = 0.0f;

	CCamera camera;
	camera.m_Position = CVector3(0.0f, 2.5f, 9.33f);

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram("example_rainbow");
	if (hShaderProgram == 0)
	{
		Error("[Batch] Failed to create shader program 'example_rainbow'\n");

		return 1;
	}

	// Create cube
	Vertex_t vertexData[] = {
		{{-0.5f, -0.5f, -0.5f}, Vertex_t::PackColor(1.0f, 0.0f, 0.0f)}, // red
		{{0.5f, -0.5f, -0.5f}, Vertex_t::PackColor(1.0f, 1.0f, 0.0f)}, // yellow
		{{0.5f, 0.5f, -0.5f}, Vertex_t::PackColor(0.0f, 1.0f, 0.0f)}, // green
		{{-0.5f, 0.5f, -0.5f}, Vertex_t::PackColor(0.0f, 0.0f, 1.0f)}, // blue

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

	// Create batch data
	BatchData_t batchData;
	CUtlVector<CQuaternion> baseRotations;
	const int32 numCubesCol = 100;
	const int32 numCubesRow = 100;
	const float32 spacing = 3.0f;
	const float32 halfWidth = (numCubesCol - 1) * 0.5f * spacing;

	for (int32 i = 0; i < numCubesRow; i++)
	{
		for (int32 j = 0; j < numCubesCol; j++)
		{
			float32 rotX = (i * 7.0f + j * 3.0f) * 5.0f;
			float32 rotY = (i * 11.0f + j * 13.0f) * 8.0f;
			float32 rotZ = (i * 17.0f + j * 19.0f) * 3.0f;
			CQuaternion rotation = CQuaternion::FromEuler(
				rotX * CMaths::Deg2Rad,
				rotY * CMaths::Deg2Rad,
				rotZ * CMaths::Deg2Rad);
			baseRotations.AddToTail(rotation);

			float32 pX = (j * spacing) - halfWidth;
			float32 pZ = (i * spacing) - halfWidth;
			batchData.AddBatch(
				CVector3(pX, 0.0f, pZ), rotation, CVector3(1.0f, 1.0f, 1.0f));
		}
	}

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

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

		rotationY += CTime::GetDeltaTime() * 30.0f;

		pRenderer->BeginFrame();

		pRenderer->Clear(ClearAll, CColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f, 0);

		// Set viewport
		Viewport_t viewport(
			0.0f, 0.0f,
			float32(windowConfig.m_Width),
			float32(windowConfig.m_Height));
		pRenderer->SetViewport(viewport);

		// Set MVP matrices
		camera.m_Position.m_X = sinf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f;
		camera.m_Position.m_Z =
			cosf(rotationY * CMaths::Deg2Rad * 0.5f) * 15.0f + 9.33f;
		float32 aspect = windowConfig.m_AspectRatio;
		CMatrix4 viewMatrix = camera.GetViewMatrix();
		CMatrix4 projMatrix = camera.GetProjectionMatrix(aspect);

		CMatrix4 mvp = projMatrix * viewMatrix;

		// Update batch data
		const uint32 batchCount =
			static_cast<uint32>(batchData.m_Transforms.Count());

		const CVector3 aabbExtent(0.5f, 0.5f, 0.5f);
		Plane_t frustumPlanes[6];
		pRenderer->ExtractFrustumPlanes(projMatrix * viewMatrix, frustumPlanes);

		const float32 nearRadius = 10.0f;
		const float32 farRadius = 500.0f;
		const float32 nearRadiusSq = nearRadius * nearRadius;
		const float32 farRadiusSq = farRadius * farRadius;

		const float32 maxAmplitude = 20.0f;
		const float32 frequency = 1.0f;

		float32 time = static_cast<float32>(CTime::GetTime() - startTime);
		
		const CQuaternion animRotation = CQuaternion::FromEuler(
			rotationX * CMaths::Deg2Rad,
			rotationY * CMaths::Deg2Rad,
			0.0f);
		
		for (uint32 i = 0; i < batchCount; i++)
		{
			BatchTransform_t& batchTransform = batchData.m_Transforms[i];

			if (!pRenderer->TestAABBFrustum(
				batchTransform.m_Position,
				aabbExtent,
				frustumPlanes))
			{
				continue;
			}

			batchTransform.m_Rotation = animRotation * baseRotations[i];

			const float32 dx = batchTransform.m_Position.m_X - camera.m_Position.m_X;
			const float32 dz = batchTransform.m_Position.m_Z - camera.m_Position.m_Z;

			const float32 distSq = dx * dx + dz * dz;

			float32 distNorm;
			if (distSq <= nearRadiusSq)
			{
				distNorm = 0.0f;
			}
			else if (distSq >= farRadiusSq)
			{
				distNorm = 1.0f;
			}
			else
			{
				const float32 dist = sqrtf(distSq);
				const float32 t = (dist - nearRadius) / (farRadius - nearRadius);
				distNorm = t * t * (3.0f - 2.0f * t);
			}
			const float32 amplitude = distNorm * maxAmplitude;
			const float32 phase =
				batchTransform.m_Position.m_X * 0.37f +
				batchTransform.m_Position.m_Z * 0.73f;
			const float32 yNorm =
				sinf((time + phase) * CMaths::TwoPI * frequency) * 0.5f +
				0.5f;
			batchTransform.m_Position.m_Y = yNorm * amplitude;
		}

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

		// Draw cubes
		pRenderer->DrawIndexedBatched(
			36,
			8,
			batchData,
			mvp,
			0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}