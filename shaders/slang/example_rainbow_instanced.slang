//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

[vk::binding(0, 0)] cbuffer Uniforms { float4x4 viewProjection; };
[vk::binding(1, 0)] cbuffer TimeParams { float4 timeParams; };
[vk::binding(2, 0)] cbuffer CameraData { float4 cameraParams; };

struct VertexInput
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(3)] float4 color : COLOR0;
    // modelCol0 = (posX, posZ, phaseSin, phaseCos)
    // modelCol1 = (qx, qy, qz, qw)
    // modelCol2 = (scaleY, lodBlendA, lodBlendB, unused)
    // modelCol3 = (lodOffsetA, lodOffsetB, lodOffsetC, unused)
    [vk::location(4)] float4 modelCol0 : TEXCOORD1;
    [vk::location(5)] float4 modelCol1 : TEXCOORD2;
    [vk::location(6)] float4 modelCol2 : TEXCOORD3;
    [vk::location(7)] float4 modelCol3 : TEXCOORD4;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [vk::location(0)] float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    float posX = input.modelCol0.x;
    float posZ = input.modelCol0.y;

    float sinVal = timeParams.x * input.modelCol0.w + timeParams.y * input.modelCol0.z;
    float posY = (sinVal * 0.5 + 0.5) * input.modelCol2.x * 20.0;

    float4 r = float4(
        timeParams.w * input.modelCol1.x + timeParams.z * input.modelCol1.z,
        timeParams.w * input.modelCol1.y + timeParams.z * input.modelCol1.w,
        timeParams.w * input.modelCol1.z - timeParams.z * input.modelCol1.x,
        timeParams.w * input.modelCol1.w - timeParams.z * input.modelCol1.y);

    float3 c1 = cross(r.xyz, input.position);
    float3 c2 = cross(r.xyz, c1);
    float3 wp = input.position + 2.0 * r.w * c1 + 2.0 * c2 + float3(posX, posY, posZ);

    wp += (input.modelCol2.xyz + input.modelCol3.xyz) * cameraParams.w;

    VertexOutput output;
    output.position = mul(float4(wp, 1.0), viewProjection);
    output.color = input.color.rgb;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}