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
uniform vec4 timeParams;
uniform vec4 cameraParams;

void main()
{
	// modelCol0 = (posX, posZ, phaseSin, phaseCos)
	// modelCol1 = (qx, qy, qz, qw)
	float posX = modelCol0.x;
	float posZ = modelCol0.y;

	// timeParams = (sinT, cosT, sinHalfY, cosHalfY)
	float sinVal = timeParams.x * modelCol0.w + timeParams.y * modelCol0.z;
	float posY = (sinVal * 0.5 + 0.5) * modelCol2.x * 20.0;

	vec4 r = vec4(
		timeParams.w * modelCol1.x + timeParams.z * modelCol1.z,
		timeParams.w * modelCol1.y + timeParams.z * modelCol1.w,
		timeParams.w * modelCol1.z - timeParams.z * modelCol1.x,
		timeParams.w * modelCol1.w - timeParams.z * modelCol1.y);

	vec3 c1 = cross(r.xyz, position);
	vec3 c2 = cross(r.xyz, c1);
	vec3 wp = position + 2.0 * r.w * c1 + 2.0 * c2 + vec3(posX, posY, posZ);

	wp += (modelCol2.xyz + modelCol3.xyz) * cameraParams.w;

	gl_Position = viewProjection * vec4(wp, 1.0);
	v_color = color.rgb;
}