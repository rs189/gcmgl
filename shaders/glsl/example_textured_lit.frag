//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#version 140

in vec3 v_position;
in vec2 v_texCoord;
in vec3 v_normal;
in vec3 v_color;

out vec4 fragColor;

// 8 lights packed as pairs: [i*2] = positionRadius, [i*2+1] = colour
uniform vec4 lightData0;
uniform vec4 lightData1;
uniform vec4 lightData2;
uniform vec4 lightData3;
uniform vec4 lightData4;
uniform vec4 lightData5;
uniform vec4 lightData6;
uniform vec4 lightData7;
uniform vec4 lightData8;
uniform vec4 lightData9;
uniform vec4 lightData10;
uniform vec4 lightData11;
uniform vec4 lightData12;
uniform vec4 lightData13;
uniform vec4 lightData14;
uniform vec4 lightData15;

uniform float numLights;

uniform sampler2D tex;

vec3 calculateLight(vec3 lightPosition, vec3 lightColor, float intensity)
{
    vec3 lightDir = lightPosition - v_position;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);

    vec3 norm = normalize(v_normal);

    // Half-lambert
    float dotNL = dot(norm, lightDir);
    float wrap = 0.5;
    float diff = (dotNL + wrap) / (1.0 + wrap);
    diff = max(diff, 0.0);

    // Distance attenuation
    float attenuation = 1.0 / (1.0 + 0.05 * distance + 0.01 * distance * distance);

    return intensity * attenuation * diff * lightColor;
}

void main()
{
    vec4 texColor = texture2D(tex, v_texCoord);

    vec3 diffuse = vec3(0.0, 0.0, 0.0);

    if (numLights > 0.0) diffuse += calculateLight(
        lightData0.xyz,
        lightData1.xyz,
        lightData0.w);
    if (numLights > 1.0) diffuse += calculateLight(
        lightData2.xyz,
        lightData3.xyz,
        lightData2.w);
    if (numLights > 2.0) diffuse += calculateLight(
        lightData4.xyz,
        lightData5.xyz,
        lightData4.w);
    if (numLights > 3.0) diffuse += calculateLight(
        lightData6.xyz,
        lightData7.xyz,
        lightData6.w);
    if (numLights > 4.0) diffuse += calculateLight(
        lightData8.xyz,
        lightData9.xyz,
        lightData8.w);
    if (numLights > 5.0) diffuse += calculateLight(
        lightData10.xyz,
        lightData11.xyz,
        lightData10.w);
    if (numLights > 6.0) diffuse += calculateLight(
        lightData12.xyz,
        lightData13.xyz,
        lightData12.w);
    if (numLights > 7.0) diffuse += calculateLight(
        lightData14.xyz,
        lightData15.xyz,
        lightData14.w);

    vec3 ambient = 0.3 * v_color;

    fragColor = vec4(texColor.rgb * (ambient + diffuse), texColor.a);
}