//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

attribute vec3 position;
attribute vec2 texCoord;

varying vec2 v_texCoord;

uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    v_texCoord = texCoord;
}