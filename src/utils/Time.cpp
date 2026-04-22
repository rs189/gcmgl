//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Time.h"
#include <sys/time.h>

#ifndef PLATFORM_PS3
#include <GLFW/glfw3.h>
#endif

float64 CTime::s_CurrentTime = 0.0;
float64 CTime::s_LastTime = 0.0;
float32 CTime::s_DeltaTime = 0.0f;
uint64 CTime::s_FrameCount = 0;
bool CTime::s_IsInitialized = false;

void CTime::Init()
{
	if (!s_IsInitialized)
	{
		s_CurrentTime = GetTimeSinceStartup();
		s_LastTime = s_CurrentTime;
		s_DeltaTime = 0.0f;
		s_FrameCount = 0;

		s_IsInitialized = true;
	}
}

void CTime::Update()
{
	if (!s_IsInitialized)
	{
		Init();
	}

	s_LastTime = s_CurrentTime;
	s_CurrentTime = GetTimeSinceStartup();

	float64 rawDelta = s_CurrentTime - s_LastTime;
	s_DeltaTime = static_cast<float32>(rawDelta);

	s_FrameCount++;
}

// Get elapsed time since startup
float64 CTime::GetTime()
{
	return s_CurrentTime;
}

float32 CTime::GetDeltaTime()
{
	return s_DeltaTime;
}

uint64 CTime::GetFrameCount()
{
	return s_FrameCount;
}

float64 CTime::GetTimeSinceStartup()
{
	timeval tv;
	gettimeofday(&tv, GCMGL_NULL);

	return float64(tv.tv_sec) + float64(tv.tv_usec) / 1000000.0;
}