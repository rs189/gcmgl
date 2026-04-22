//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef TIME_H
#define TIME_H

#pragma once

#include "tier0/platform.h"

class CTime
{
public:
	CTime();
	~CTime();

	static void Init();
	static void Update();
	static float64 GetTime();
	static float32 GetDeltaTime();
	static uint64 GetFrameCount();
	static float64 GetTimeSinceStartup();
private:
	static float64 s_CurrentTime;
	static float64 s_LastTime;
	static float32 s_DeltaTime;
	static uint64 s_FrameCount;
	static bool s_IsInitialized;
};

#endif // TIME_H