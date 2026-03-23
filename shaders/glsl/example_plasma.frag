//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 120

varying vec3 v_objPos;

uniform float time;

void main()
{
    float x = v_objPos.x;
    float y = v_objPos.y;
    float z = v_objPos.z;

    float v = sin(x * 6.0 + time * 1.5)
            + sin(y * 6.0 + time * 1.2)
            + sin((x + y + z) * 4.0 + time)
            + sin(sqrt(x * x + y * y + z * z + 1.0) * 5.0 - time * 2.0);

    v = v * 0.25 + 0.5;

    float r = sin(v * 3.14159);
    float g = sin(v * 3.14159 + 2.094);
    float b = sin(v * 3.14159 + 4.189);

    gl_FragColor = vec4(r, g, b, 1.0);
}