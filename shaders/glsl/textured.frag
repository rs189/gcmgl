//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

varying vec2 v_texCoord;

uniform sampler2D tex;

void main()
{
    gl_FragColor = texture2D(tex, v_texCoord);
}