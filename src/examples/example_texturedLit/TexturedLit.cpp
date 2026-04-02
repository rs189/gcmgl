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
#include <math.h>

#ifdef PLATFORM_PS3
#include <sysmodule/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#else // PLATFORM_PS3
#include <GLFW/glfw3.h>
#endif // !PLATFORM_PS3

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

struct TexturedLitVertex_t
{
	CVector3 m_Position;
	CVector2 m_TexCoord;
	CVector3 m_Normal;
	uint32 m_Color;

	TexturedLitVertex_t() :
		m_Position(0.0f, 0.0f, 0.0f),
		m_TexCoord(0.0f, 0.0f),
		m_Normal(0.0f, 0.0f, 0.0f),
		m_Color(0)
	{
	}

	TexturedLitVertex_t(
		const CVector3& position,
		const CVector2& texCoord,
		const CVector3& normal,
		uint32 color) :
		m_Position(position),
		m_TexCoord(texCoord),
		m_Normal(normal),
		m_Color(color)
	{
	}
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

int32 RunTexturedLitExample(
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

	// Create cube with per-face normals
	TexturedLitVertex_t vertexData[] = {
		// front (z = -0.5)
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, -0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, -0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, -0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, -0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),

		// back (z = 0.5)
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, 0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, 0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, 0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, 0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),

		// right (x = 0.5)
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, -0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, 0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, 0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, -0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),

		// left (x = -0.5)
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, 0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, -0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, -0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, 0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),

		// top (y = 0.5)
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, -0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, -0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, 0.5f, 0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, 0.5f, 0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, 1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),

		// bottom (y = -0.5)
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, 0.5f), CVector2(0.0f, 0.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, 0.5f), CVector2(1.0f, 0.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, 1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(0.5f, -0.5f, -0.5f), CVector2(1.0f, 1.0f), CMaths::Normalise(CVector3(1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f)),
		TexturedLitVertex_t(CVector3(-0.5f, -0.5f, -0.5f), CVector2(0.0f, 1.0f), CMaths::Normalise(CVector3(-1.0f, -1.0f, -1.0f)), Vertex_t::PackColor(1.0f, 1.0f, 1.0f))
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
#endif // PLATFORM_PS3
	uint8* pRawImageData = stbi_load(
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

		Msg("[TexturedLit] Loaded texture: %dx%d\n", texWidth, texHeight);
	}
	else
	{
		Error("[TexturedLit] Failed to load texture.png\n");
		
		return 1;
	}

	// Create shader
	ShaderProgramHandle hShaderProgram =
		pRenderer->CreateShaderProgram("example_textured_lit");
	if (hShaderProgram == 0)
	{
		Error("[TexturedLit] Failed to create shader program 'example_textured_lit'\n");
		
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
	vertexLayout.SetStride(sizeof(TexturedLitVertex_t));

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

		rotationX += CTime::GetDeltaTime() * 30.0f;
		rotationY += CTime::GetDeltaTime() * 20.0f;
		lightAngle += CTime::GetDeltaTime() * 15.0f;

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
		camera.m_Position = CVector3(0.0f, 0.0f, 5.0f);
		float32 aspectRatio = windowConfig.m_AspectRatio;
		CMatrix4 projectionMatrix = camera.GetProjectionMatrix(aspectRatio);
		CMatrix4 viewMatrix = camera.GetViewMatrix();
		CMatrix4 model =
			CMaths::Rotate(
				CMatrix4(1.0f),
				rotationY,
				CVector3(0.0f, 1.0f, 0.0f)) *
			CMaths::Rotate(
				CMatrix4(1.0f),
				rotationX,
				CVector3(1.0f, 0.0f, 0.0f));
		CMatrix4 mvp = projectionMatrix * viewMatrix * model;
		
		// Update constant buffers
		pRenderer->UpdateBuffer(hMVPConstantBuffer, &mvp, sizeof(CMatrix4));
		pRenderer->UpdateBuffer(
			hModelConstantBuffer,
			&model,
			sizeof(CMatrix4));

		// Set up three colored point lights rotating around the cube
		LightData_t lightData;
		lightData.m_NumLights = 3.0f;

		// Red light
		lightData.SetLight(
			0,
			CVector4(
				lightRadius * cosf(lightAngle),
				0.0f,
				lightRadius * sinf(lightAngle),
				lightIntensity),
			CVector4(1.0f, 0.0f, 0.0f, 1.0f));

		// Green light
		lightData.SetLight(
			1,
			CVector4(
				lightRadius * cosf(lightAngle + twoPiThirds),
				0.0f,
				lightRadius * sinf(lightAngle + twoPiThirds),
				lightIntensity),
			CVector4(0.0f, 1.0f, 0.0f, 1.0f));

		// Blue light
		lightData.SetLight(
			2,
			CVector4(
				lightRadius * cosf(-lightAngle + fourPiThirds),
				0.0f,
				lightRadius * sinf(-lightAngle + fourPiThirds),
				lightIntensity),
			CVector4(0.0f, 0.0f, 1.0f, 1.0f));

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

		// Bind texture
		pRenderer->SetTexture(hTexture, 0, ShaderStageFragment);

		// Draw cube
		pRenderer->DrawIndexed(36, 0);

		pRenderer->EndFrame();
	}

	// Cleanup
	pRenderer->DestroyBuffer(hVertexBuffer);
	pRenderer->DestroyBuffer(hIndexBuffer);
	pRenderer->DestroyBuffer(hMVPConstantBuffer);
	pRenderer->DestroyBuffer(hModelConstantBuffer);
	pRenderer->DestroyBuffer(hLightConstantBuffer);
	pRenderer->DestroyBuffer(hNumLightsConstantBuffer);
	pRenderer->DestroyTexture(hTexture);
	pRenderer->DestroyShaderProgram(hShaderProgram);

	return 0;
}