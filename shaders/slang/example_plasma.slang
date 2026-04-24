//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

[vk::binding(0, 0)] cbuffer Uniforms { float4x4 mvp; };
[vk::binding(1, 0)] cbuffer TimeParams { float time; };

struct VertexInput
{
    [vk::location(0)] float3 position : POSITION;
    [vk::location(3)] float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [vk::location(0)] float3 objPos : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.objPos = input.position;

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    float x = input.objPos.x;
    float y = input.objPos.y;
    float z = input.objPos.z;

    float v = sin(x * 6.0 + time * 1.5)
            + sin(y * 6.0 + time * 1.2)
            + sin((x + y + z) * 4.0 + time)
            + sin(sqrt(x * x + y * y + z * z + 1.0) * 5.0 - time * 2.0);

    v = v * 0.25 + 0.5;

    float r = sin(v * 3.14159);
    float g = sin(v * 3.14159 + 2.094);
    float b = sin(v * 3.14159 + 4.189);

    return float4(r, g, b, 1.0);
}