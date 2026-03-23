//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

varying vec3 v_color;

void main()
{
    gl_FragColor = vec4(v_color, 1.0);
}