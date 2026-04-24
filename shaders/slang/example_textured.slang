//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

[vk::binding(0, 0)] cbuffer Uniforms { float4x4 mvp; };

struct VertexInput
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(2)] float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [vk::location(0)] float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.texCoord = input.texCoord;
    return output;
}

[vk::binding(0, 0)] uniform Sampler2D tex;

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return tex.Sample(input.texCoord);
}