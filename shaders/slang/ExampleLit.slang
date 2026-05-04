//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

[vk::binding(0, 0)] cbuffer Uniforms
{
    float4x4 Mvp;
};
[vk::binding(2, 0)] cbuffer ModelData
{
    float4x4 Model;
};

struct VertexInput_t
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(1)] float3 normal : NORMAL;
    [vk::location(2)] float2 texCoord : TEXCOORD0;
    [vk::location(3)] float4 color : COLOR0;
};

struct VertexOutput_t
{
    float4 position : SV_Position;
    [vk::location(0)] float3 worldPos : TEXCOORD0;
    [vk::location(1)] float3 normal : TEXCOORD1;
    [vk::location(2)] float3 color : TEXCOORD2;
};

[shader("vertex")]
VertexOutput_t vertMain(VertexInput_t input)
{
    VertexOutput_t output;
    float4 worldPos = mul(float4(input.position, 1.0), Model);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3)Model));
    output.color = input.color.rgb;
    output.position = mul(float4(input.position, 1.0), Mvp);

    return output;
}

[vk::binding(1, 0)] cbuffer LightData
{
    float4 LightData0;
    float4 LightData1;
    float4 LightData2;
    float4 LightData3;
    float4 LightData4;
    float4 LightData5;
    float4 LightData6;
    float4 LightData7;
    float4 LightData8;
    float4 LightData9;
    float4 LightData10;
    float4 LightData11;
    float4 LightData12;
    float4 LightData13;
    float4 LightData14;
    float4 LightData15;
};

[vk::binding(3, 0)] cbuffer NumLights
{
    float NumLights;
};

float3 CalculateLight(
    float3 worldPos,
    float3 normal,
    float3 baseColor,
    float3 lightPosition,
    float3 lightColor,
    float intensity)
{
    float3 lightDir = lightPosition - worldPos;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);
    float3 norm = normalize(normal);
    float dotNL = dot(norm, lightDir);
    float wrap = 0.5;
    float diff = (dotNL + wrap) / (1.0 + wrap);
    diff = max(diff, 0.0);
    float attenuation = 1.0 / (1.0 + 0.05 * distance + 0.01 * distance * distance);

    return intensity * attenuation * diff * lightColor * baseColor;
}

[shader("fragment")]
float4 fragMain(VertexOutput_t input) : SV_Target
{
    float3 diffuse = float3(0.0, 0.0, 0.0);
    if (NumLights > 0.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData0.xyz,
            LightData1.xyz,
            LightData0.w);
    }
    if (NumLights > 1.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData2.xyz,
            LightData3.xyz,
            LightData2.w);
    }
    if (NumLights > 2.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData4.xyz,
            LightData5.xyz,
            LightData4.w);
    }
    if (NumLights > 3.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData6.xyz,
            LightData7.xyz,
            LightData6.w);
    }
    if (NumLights > 4.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData8.xyz,
            LightData9.xyz,
            LightData8.w);
    }
    if (NumLights > 5.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData10.xyz,
            LightData11.xyz,
            LightData10.w);
    }
    if (NumLights > 6.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData12.xyz,
            LightData13.xyz,
            LightData12.w);
    }
    if (NumLights > 7.0)
    {
        diffuse += CalculateLight(
            input.worldPos,
            input.normal,
            input.color,
            LightData14.xyz,
            LightData15.xyz,
            LightData14.w);
    }
    float3 ambient = 0.3 * input.color;

    return float4(ambient + diffuse, 1.0);
}