#pragma once

#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <rsx/commands.h>
#include <sysutil/video.h>

#define FRAME_BUFFER_COUNT	2
#define DEFUALT_CB_SIZE		0x100000

extern videoResolution vResolution;
extern gcmContextData *context;

extern u32 curr_fb;
extern u32 first_fb;

extern u32 display_width;
extern u32 display_height;

extern u32 depth_pitch;
extern u32 depth_offset;
extern u32 *depth_buffer;

extern u32 color_pitch;
extern u32 color_offset[FRAME_BUFFER_COUNT];
extern u32 *color_buffer[FRAME_BUFFER_COUNT];

extern f32 aspect_ratio;

void initVideoConfiguration();
void setRenderTarget(u32 index);
void setDrawEnv();
void initScreen(void *host_addr, u32 size);
void waitFlip();
void flip();
void waitFinish();