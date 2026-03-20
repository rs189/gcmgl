//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

varying vec3 v_color;

uniform float time;

void main()
{
    // Create a wave pattern based on time
    float wave = sin(time * 3.0 + gl_FragCoord.x * 0.01 + gl_FragCoord.y * 0.01) * 0.5 + 0.5;
    vec3 animatedColor = mix(v_color, vec3(1.0) - v_color, wave);
    gl_FragColor = vec4(animatedColor, 1.0);
}