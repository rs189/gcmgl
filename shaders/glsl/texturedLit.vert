//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 140

in vec3 position;
in vec2 texCoord;
in vec3 normal;
in vec4 color;

out vec3 v_position;
out vec2 v_texCoord;
out vec3 v_normal;
out vec3 v_color;

uniform mat4 mvp;
uniform mat4 model;

void main()
{
    // World space
    vec4 worldPos = model * vec4(position, 1.0);
    v_position = worldPos.xyz;

    // Transform normal using inverse transpose
    v_normal = normalize(mat3(model) * normal);

    v_texCoord = texCoord;
    v_color = color.rgb;
    gl_Position = mvp * vec4(position, 1.0);
}