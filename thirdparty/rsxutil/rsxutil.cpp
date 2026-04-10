#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>

#include <sysutil/video.h>

#include "rsxutil.h"

#define GCM_LABEL_INDEX		255

videoResolution vResolution;
gcmContextData *context = NULL;

u32 curr_fb = 0;
u32 first_fb = 1;

u32 display_width;
u32 display_height;

u32 depth_pitch;
u32 depth_offset;
u32 *depth_buffer;

u32 color_pitch;
u32 color_offset[FRAME_BUFFER_COUNT];
u32 *color_buffer[FRAME_BUFFER_COUNT];

f32 aspect_ratio;

static u32 sResolutionIds[] = {
    VIDEO_RESOLUTION_960x1080,
    VIDEO_RESOLUTION_720,
    VIDEO_RESOLUTION_480,
    VIDEO_RESOLUTION_576
};
static size_t RESOLUTION_ID_COUNT = sizeof(sResolutionIds)/sizeof(u32);

static u32 sLabelVal = 1;

void waitFinish()
{
	rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, sLabelVal);
	rsxSetWaitLabel(context, GCM_LABEL_INDEX, sLabelVal);

	rsxFlushBuffer(context);

	while (*(vu32*)gcmGetLabelAddress(GCM_LABEL_INDEX) != sLabelVal)
		usleep(30);

	++sLabelVal;
}

void initVideoConfiguration()
{
    s32 rval = 0;
    s32 resId = 0;

    for (size_t i=0;i < RESOLUTION_ID_COUNT;i++) {
        rval = videoGetResolutionAvailability(VIDEO_PRIMARY, sResolutionIds[i], VIDEO_ASPECT_AUTO, 0);
        if (rval != 1) continue;

        resId = sResolutionIds[i];
        rval = videoGetResolution(resId, &vResolution);
        if(!rval) break;
    }

    if(rval) {
        printf("Error: videoGetResolutionAvailability failed. No usable resolution.\n");
        exit(1);
    }

    videoConfiguration config = {
        (u8)resId,
        VIDEO_BUFFER_FORMAT_XRGB,
        VIDEO_ASPECT_AUTO,
        {0,0,0,0,0,0,0,0,0},
        (u32)vResolution.width*4
    };

    rval = videoConfigure(VIDEO_PRIMARY, &config, NULL, 0);
    if(rval) {
        printf("Error: videoConfigure failed.\n");
        exit(1);
    }

    videoState state;

    rval = videoGetState(VIDEO_PRIMARY, 0, &state);
    switch(state.displayMode.aspect) {
        case VIDEO_ASPECT_4_3:
            aspect_ratio = 4.0f/3.0f;
            break;
        case VIDEO_ASPECT_16_9:
            aspect_ratio = 16.0f/9.0f;
            break;
        default:
            printf("unknown aspect ratio %x\n", state.displayMode.aspect);
            aspect_ratio = 16.0f/9.0f;
            break;
    }

    display_height = vResolution.height;
    display_width = vResolution.width;
}

void setRenderTarget(u32 index)
{
	gcmSurface sf;

	sf.colorFormat		= GCM_SURFACE_X8R8G8B8;
	sf.colorTarget		= GCM_SURFACE_TARGET_0;
	sf.colorLocation[0]	= GCM_LOCATION_RSX;
	sf.colorOffset[0]	= color_offset[index];
	sf.colorPitch[0]	= color_pitch;

	sf.colorLocation[1]	= GCM_LOCATION_RSX;
	sf.colorLocation[2]	= GCM_LOCATION_RSX;
	sf.colorLocation[3]	= GCM_LOCATION_RSX;
	sf.colorOffset[1]	= 0;
	sf.colorOffset[2]	= 0;
	sf.colorOffset[3]	= 0;
	sf.colorPitch[1]	= color_pitch;
	sf.colorPitch[2]	= color_pitch;
	sf.colorPitch[3]	= color_pitch;

	sf.depthFormat		= GCM_SURFACE_ZETA_Z24S8;
	sf.depthLocation	= GCM_LOCATION_RSX;
	sf.depthOffset		= depth_offset;
	sf.depthPitch		= depth_pitch;

	sf.type				= GCM_SURFACE_TYPE_LINEAR;
	sf.antiAlias		= GCM_SURFACE_CENTER_1;

	sf.width			= display_width;
	sf.height			= display_height;
	sf.x				= 0;
	sf.y				= 0;

	rsxSetSurface(context,&sf);
}

void setDrawEnv()
{
	rsxSetColorMask(context,GCM_COLOR_MASK_B |
							GCM_COLOR_MASK_G |
							GCM_COLOR_MASK_R |
							GCM_COLOR_MASK_A);

	rsxSetColorMaskMrt(context,0);

	u16 x,y,w,h;
	f32 min, max;
	f32 scale[4],offset[4];

	x = 0;
	y = 0;
	w = display_width;
	h = display_height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = w*0.5f;
	scale[1] = h*-0.5f;
	scale[2] = (max - min)*0.5f;
	scale[3] = 0.0f;
	offset[0] = x + w*0.5f;
	offset[1] = y + h*0.5f;
	offset[2] = (max + min)*0.5f;
	offset[3] = 0.0f;

	rsxSetViewport(context,x, y, w, h, min, max, scale, offset);
	rsxSetScissor(context,x,y,w,h);

	rsxSetDepthTestEnable(context,GCM_TRUE);
	rsxSetDepthFunc(context,GCM_LESS);
	rsxSetShadeModel(context,GCM_SHADE_MODEL_SMOOTH);
	rsxSetDepthWriteEnable(context,1);
	rsxSetFrontFace(context,GCM_FRONTFACE_CCW);

	rsxSetCullFaceEnable(context, GCM_FALSE);

	rsxSetZMinMaxControl(context, 0, 1, 1);

	for (int i = 0; i < 8; ++i) {
		rsxSetViewportClip(context, i, display_width, display_height);
	}

	rsxSetUserClipPlaneControl(context,
							   GCM_USER_CLIP_PLANE_DISABLE,
							   GCM_USER_CLIP_PLANE_DISABLE,
							   GCM_USER_CLIP_PLANE_DISABLE,
							   GCM_USER_CLIP_PLANE_DISABLE,
							   GCM_USER_CLIP_PLANE_DISABLE,
							   GCM_USER_CLIP_PLANE_DISABLE);
}

void initScreen(void *host_addr,u32 size)
{
    u32 zs_depth = 4;
    u32 color_depth = 4;

	rsxInit(&context,DEFUALT_CB_SIZE,size,host_addr);

	initVideoConfiguration();

	waitFinish();

	gcmSetFlipMode(GCM_FLIP_VSYNC);

	color_pitch = display_width*color_depth;
	depth_pitch = display_width*zs_depth;

	for (u32 i=0;i < FRAME_BUFFER_COUNT;i++) {
		color_buffer[i] = (u32*)rsxMemalign(64,(display_height*color_pitch));
		rsxAddressToOffset(color_buffer[i],&color_offset[i]);
		gcmSetDisplayBuffer(i,color_offset[i],color_pitch,display_width,display_height);
	}

	depth_buffer = (u32*)rsxMemalign(64, display_height*depth_pitch);
	rsxAddressToOffset(depth_buffer,&depth_offset);
}

void waitFlip()
{
	while(gcmGetFlipStatus()!=0)
		usleep(200);
	gcmResetFlipStatus();
}

void flip()
{
	if(!first_fb) waitFlip();
	else gcmResetFlipStatus();

	rsxFlushBuffer(context);

	gcmSetFlip(context, curr_fb);
	rsxFlushBuffer(context);

	gcmSetWaitFlip(context);
	//rsxFlushBuffer(context);

	curr_fb ^= 1;
	setRenderTarget(curr_fb);

	first_fb = 0;
}