//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

attribute vec3 position;
attribute vec4 color;
attribute vec4 modelCol0;
attribute vec4 modelCol1;
attribute vec4 modelCol2;
attribute vec4 modelCol3;

varying vec3 v_color;

uniform mat4 viewProjection;

void main()
{
    mat4 model = mat4(modelCol0, modelCol1, modelCol2, modelCol3);
    gl_Position = viewProjection * model * vec4(position, 1.0);
    v_color = color.rgb;
}