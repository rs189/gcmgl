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

struct VertexInput
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(3)] float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [vk::location(0)] float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), Mvp);
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}