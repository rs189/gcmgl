//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

[vk::binding(0, 0)] cbuffer Uniforms
{
    float4x4 mvp;
};
[vk::binding(2, 0)] cbuffer ModelData
{
    float4x4 model;
};

struct VertexInput
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(1)] float3 normal : NORMAL;
    [vk::location(2)] float2 texCoord : TEXCOORD0;
    [vk::location(3)] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [vk::location(0)] float3 worldPos : TEXCOORD0;
    [vk::location(1)] float3 normal : TEXCOORD1;
    [vk::location(2)] float2 texCoord : TEXCOORD2;
    [vk::location(3)] float3 color : TEXCOORD3;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPos = mul(float4(input.position, 1.0), model);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3)model));
    output.texCoord = input.texCoord;
    output.color = input.color.rgb;
    output.position = mul(float4(input.position, 1.0), mvp);

    return output;
}

[vk::binding(1, 0)] cbuffer LightData
{
    float4 lightData0;
    float4 lightData1;
    float4 lightData2;
    float4 lightData3;
    float4 lightData4;
    float4 lightData5;
    float4 lightData6;
    float4 lightData7;
    float4 lightData8;
    float4 lightData9;
    float4 lightData10;
    float4 lightData11;
    float4 lightData12;
    float4 lightData13;
    float4 lightData14;
    float4 lightData15;
};

[vk::binding(3, 0)] cbuffer NumLights
{
    float numLights;
};

[vk::binding(0, 0)] uniform Sampler2D tex;

float3 calculateLight(
    float3 worldPos,
    float3 normal,
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

    return intensity * attenuation * diff * lightColor;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    float4 texColor = tex.Sample(input.texCoord);
    float3 diffuse = float3(0.0, 0.0, 0.0);
    if (numLights > 0.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData0.xyz,
            lightData1.xyz,
            lightData0.w);
    }
    if (numLights > 1.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData2.xyz,
            lightData3.xyz,
            lightData2.w);
    }
    if (numLights > 2.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData4.xyz,
            lightData5.xyz,
            lightData4.w);
    }
    if (numLights > 3.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData6.xyz,
            lightData7.xyz,
            lightData6.w);
    }
    if (numLights > 4.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData8.xyz,
            lightData9.xyz,
            lightData8.w);
    }
    if (numLights > 5.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData10.xyz,
            lightData11.xyz,
            lightData10.w);
    }
    if (numLights > 6.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData12.xyz,
            lightData13.xyz,
            lightData12.w);
    }
    if (numLights > 7.0)
    {
        diffuse += calculateLight(
            input.worldPos,
            input.normal,
            lightData14.xyz,
            lightData15.xyz,
            lightData14.w);
    }
    float3 ambient = 0.3 * input.color;

    return float4(texColor.rgb * (ambient + diffuse), texColor.a);
}