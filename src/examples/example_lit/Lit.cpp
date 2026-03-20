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
#include <math.h>

#ifdef PLATFORM_PS3
#include <sysmodule/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#else
#include <GLFW/glfw3.h>
#endif

struct LitVertex_t
{
	CVector3 m_Position;
	CVector2 m_TexCoord;
	CVector3 m_Normal;
	uint32 m_Color;
};

#define MAX_LIGHTS 8

struct LightData_t
{
	CVector4 m_Slots[MAX_LIGHTS * 2];
	float32 m_NumLights;

	void SetLight(
		int32 index,
		const CVector4& positionRadius,
		const CVector4& colourIntensity)
	{
		m_Slots[index * 2] = positionRadius;
		m_Slots[index * 2 + 1] = colourIntensity;
	}
};

int32 RunLitExample(
	CWindowManager& windowManager,
	IRenderer* pRenderer,
	IWindow* pWindow,
	const WindowConfig_t& windowConfig)
{
	bool isRunning = true;

	CTime::Initialize();

	float32 rotationY = 0.0f;
	float32 rotationX = 0.0f;

	CCamera camera;

	// Create shader
	ShaderProgramHandle hShaderProgram = pRenderer->CreateShaderProgram("lit");
	if (hShaderProgram == 0)
	{
		printf("[ERROR][Lit] Failed to create shader program 'lit'\n");

		return 1;
	}

	// Create sphere with smooth normals
	const int32 segments = 32;
	const int32 rings = 16;
	const float32 radius = 1.0f;

	const int32 vertexCount = (rings + 1) * (segments + 1);
	const int32 indexCount = rings * segments * 6;

	LitVertex_t* pSphereVertices = new LitVertex_t[vertexCount];
	uint32* pSphereIndices = new uint32[indexCount];

	int32 vertexIdx = 0;

	for (int32 ring = 0; ring <= rings; ring++)
	{
		float32 theta = float32(ring) * 3.14159f / float32(rings);
		float32 sinTheta = sinf(theta);
		float32 cosTheta = cosf(theta);

		for (int32 seg = 0; seg <= segments; seg++)
		{
			float32 phi = float32(seg) * 2.0f * 3.14159f / float32(segments);
			float32 sinPhi = sinf(phi);
			float32 cosPhi = cosf(phi);

			CVector3 position(
				radius * sinTheta * cosPhi,
				radius * cosTheta,
				radius * sinTheta * sinPhi);

			CVector3 normal = position;
			float32 len = normal.Length();
			if (len > 0.0f)
			{
				normal.m_X /= len;
				normal.m_Y /= len;
				normal.m_Z /= len;
			}

			pSphereVertices[vertexIdx].m_Position = position;
			pSphereVertices[vertexIdx].m_TexCoord = CVector2(0.0f, 0.0f);
			pSphereVertices[vertexIdx].m_Normal = normal;
			pSphereVertices[vertexIdx].m_Color = Vertex_t::PackColor(
				0.1f,
				0.1f,
				0.5f);

			vertexIdx++;
		}
	}
	
	int32 indexIdx = 0;
	for (int32 ring = 0; ring < rings; ring++)
	{
		for (int32 seg = 0; seg < segments; seg++)
		{
			int32 current = ring * (segments + 1) + seg;
			int32 next = current + segments + 1;
			
			pSphereIndices[indexIdx++] = uint32(current);
			pSphereIndices[indexIdx++] = uint32(next);
			pSphereIndices[indexIdx++] = uint32(current + 1);
			
			pSphereIndices[indexIdx++] = uint32(current + 1);
			pSphereIndices[indexIdx++] = uint32(next);
			pSphereIndices[indexIdx++] = uint32(next + 1);
		}
	}

	// Create buffers
	BufferHandle hVertexBuffer = pRenderer->CreateVertexBuffer(
		pSphereVertices,
		uint64(vertexCount * sizeof(LitVertex_t)),
		BufferUsage_t::Static);
	BufferHandle hIndexBuffer = pRenderer->CreateIndexBuffer(
		pSphereIndices,
		uint64(indexCount * sizeof(uint32)),
		IndexFormat_t::UInt32,
		BufferUsage_t::Static);
	BufferHandle hMVPConstantBuffer = pRenderer->CreateConstantBuffer(
		sizeof(CMatrix4),
		BufferUsage_t::Dynamic);
	BufferHandle hModelConstantBuffer = pRenderer->CreateConstantBuffer(
		sizeof(CMatrix4),
		BufferUsage_t::Dynamic);
	BufferHandle hLightConstantBuffer = pRenderer->CreateConstantBuffer(
		sizeof(CVector4) * MAX_LIGHTS * 2,
		BufferUsage_t::Dynamic);
	BufferHandle hNumLightsConstantBuffer = pRenderer->CreateConstantBuffer(
		sizeof(float32),
		BufferUsage_t::Dynamic);

	// Create uniform layout for MVP matrix
	UniformBlockLayout_t mvpLayout;
	mvpLayout.m_UniformNames.AddToTail("mvp");
	mvpLayout.m_Binding = 0;
	mvpLayout.m_Size = sizeof(CMatrix4);
	uint32 hMVPLayout = pRenderer->CreateUniformBlockLayout(mvpLayout);

	// Create uniform layout for model matrix
	UniformBlockLayout_t modelLayout;
	modelLayout.m_UniformNames.AddToTail("model");
	modelLayout.m_Binding = 2;
	modelLayout.m_Size = sizeof(CMatrix4);
	uint32 hModelLayout = pRenderer->CreateUniformBlockLayout(modelLayout);

	// Create uniform layout for light data (16 vec4 slots + numLights)
	UniformBlockLayout_t lightLayout;
	lightLayout.m_Binding = 1;
	lightLayout.m_Size = sizeof(CVector4) * MAX_LIGHTS * 2;
	lightLayout.m_UniformNames.AddToTail("lightData0");
	lightLayout.m_UniformNames.AddToTail("lightData1");
	lightLayout.m_UniformNames.AddToTail("lightData2");
	lightLayout.m_UniformNames.AddToTail("lightData3");
	lightLayout.m_UniformNames.AddToTail("lightData4");
	lightLayout.m_UniformNames.AddToTail("lightData5");
	lightLayout.m_UniformNames.AddToTail("lightData6");
	lightLayout.m_UniformNames.AddToTail("lightData7");
	lightLayout.m_UniformNames.AddToTail("lightData8");
	lightLayout.m_UniformNames.AddToTail("lightData9");
	lightLayout.m_UniformNames.AddToTail("lightData10");
	lightLayout.m_UniformNames.AddToTail("lightData11");
	lightLayout.m_UniformNames.AddToTail("lightData12");
	lightLayout.m_UniformNames.AddToTail("lightData13");
	lightLayout.m_UniformNames.AddToTail("lightData14");
	lightLayout.m_UniformNames.AddToTail("lightData15");
	uint32 hLightLayout = pRenderer->CreateUniformBlockLayout(lightLayout);

	UniformBlockLayout_t numLightsLayout;
	numLightsLayout.m_UniformNames.AddToTail("numLights");
	numLightsLayout.m_Binding = 3;
	numLightsLayout.m_Size = sizeof(float32);
	uint32 hNumLightsLayout =
		pRenderer->CreateUniformBlockLayout(numLightsLayout);

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
		"normal",
		uint32(VertexFormat_t::Float3),
		sizeof(CVector3) + sizeof(CVector2),
		VertexSemantic_t::Normal);
	vertexLayout.AddAttribute(
		"color",
		uint32(VertexFormat_t::UByte4_Norm),
		sizeof(CVector3) + sizeof(CVector2) + sizeof(CVector3),
		VertexSemantic_t::Color0);
	vertexLayout.SetStride(sizeof(LitVertex_t));

	float32 lightAngle = 0.0f;

	// Light constants
	const float32 lightRadius = 3.0f;
	const float32 lightIntensity = 10.0f;
	const float32 twoPiThirds = CMaths::PI * 2.0f / 3.0f;
	const float32 fourPiThirds = CMaths::PI * 4.0f / 3.0f;

	while (isRunning)
	{
		pWindow->PollEvents();
		if (pWindow->ShouldClose())
		{
			isRunning = false;
		}

		CTime::Update();

		rotationY += CTime::GetDeltaTime() * 30.0f;
		rotationX += CTime::GetDeltaTime() * 15.0f;
		lightAngle += CTime::GetDeltaTime() * 15.0f;

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
		CMatrix4 view = CMaths::LookAt(
			camera.m_Position,
			CVector3(0.0f, 0.0f, 0.0f),
			CVector3(0.0f, 1.0f, 0.0f));

		CMatrix4 model =
			CMaths::Rotate(
				CMatrix4(1.0f), rotationY, CVector3(0.0f, 1.0f, 0.0f)) *
			CMaths::Rotate(
				CMatrix4(1.0f), rotationX, CVector3(1.0f, 0.0f, 0.0f));
		CMatrix4 mvp = proj * view * model;

		// Set up three colored point lights rotating around the sphere
		LightData_t lightData;
		lightData.m_NumLights = 3.0f;

		// Red light
		lightData.SetLight(0,
			CVector4(
				lightRadius * cosf(lightAngle), 
				0.0f,
				lightRadius * sinf(lightAngle), 
				lightIntensity),
			CVector4(1.0f, 0.0f, 0.0f, 1.0f));

		// Green light
		lightData.SetLight(1,
			CVector4(
				lightRadius * cosf(lightAngle + twoPiThirds),
				0.0f,
				lightRadius * sinf(lightAngle + twoPiThirds),
				lightIntensity),
			CVector4(0.0f, 1.0f, 0.0f, 1.0f));

		// Blue light
		lightData.SetLight(2,
			CVector4(
				lightRadius * cosf(-lightAngle + fourPiThirds),
				0.0f,
				lightRadius * sinf(-lightAngle + fourPiThirds),
				lightIntensity),
			CVector4(0.0f, 0.0f, 1.0f, 1.0f));

		// Update constant buffers
		pRenderer->UpdateBuffer(hMVPConstantBuffer, &mvp, sizeof(CMatrix4));
		pRenderer->UpdateBuffer(
			hModelConstantBuffer,
			&model,
			sizeof(CMatrix4));
		pRenderer->UpdateBuffer(
			hLightConstantBuffer,
			lightData.m_Slots,
			sizeof(CVector4) * MAX_LIGHTS * 2);
		pRenderer->UpdateBuffer(
			hNumLightsConstantBuffer,
			&lightData.m_NumLights,
			sizeof(float32));

		// Bind shader and buffers
		pRenderer->SetShaderProgram(hShaderProgram);

		pRenderer->SetConstantBuffer(
			hMVPConstantBuffer,
			hMVPLayout,
			0,
			ShaderStageVertex);
		pRenderer->SetConstantBuffer(
			hModelConstantBuffer,
			hModelLayout,
			2,
			ShaderStageVertex);
		pRenderer->SetConstantBuffer(
			hLightConstantBuffer,
			hLightLayout,
			1,
			ShaderStageFragment);
		pRenderer->SetConstantBuffer(
			hNumLightsConstantBuffer,
			hNumLightsLayout,
			3,
			ShaderStageFragment);
		pRenderer->SetVertexBuffer(
			hVertexBuffer,
			0,
			vertexLayout.GetStride(),
			0,
			&vertexLayout);
		pRenderer->SetIndexBuffer(hIndexBuffer);

		// Draw sphere
		pRenderer->DrawIndexed(uint32(indexCount), 0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hMVPConstantBuffer);
	pRenderer->DestroyBuffer(hModelConstantBuffer);
	pRenderer->DestroyBuffer(hLightConstantBuffer);
	pRenderer->DestroyBuffer(hNumLightsConstantBuffer);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	delete[] pSphereVertices;
	delete[] pSphereIndices;

	return 0;
}