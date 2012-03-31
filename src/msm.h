/* msm.h
 *
 * Copyright (c) 2009-2010 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MSM_H_
#define _MSM_H_

#include "xf86.h"
#include "damage.h"
#include "exa.h"

#include <linux/fb.h>
#include <linux/ioctl.h>
#include <linux/msm_mdp.h>

#define ARRAY_SIZE(a) (sizeof((a)) / (sizeof(*(a))))

/* This enumerates all of the available options */

typedef enum
{
	OPTION_FB,
	OPTION_NOACCEL,
	OPTION_SWCURSOR,
	OPTION_VSYNC,
	OPTION_FBCACHE,
	OPTION_PIXMAP_MEMTYPE,
	OPTION_PAGEFLIP,
	OPTION_DRIMEMTYPE,
	OPTION_DEBUG,
} MSMOpts;

typedef enum
{
	MSM_MDP_VERSION_22,
	MSM_MDP_VERSION_31,
	MSM_MDP_VERSION_40,
} MSMChipType;

struct kgsl_ringbuffer;

typedef struct _MSMRec
{
	/* File descriptor for the framebuffer device */
	int fd;

	/* Fixed and var strutures from the framebuffer */
	struct fb_fix_screeninfo fixed_info;
	struct fb_var_screeninfo mode_info;

	/* Pointer to the mapped framebuffer memory */
	void *fbmem;

	/* Processor identifier */
	MSMChipType chipID;

	/* Default mode for X */
	DisplayModeRec default_mode;

	/* EXA driver structure */
	ExaDriverPtr pExa;

	/* Place holder for the standard close screen function */
	CloseScreenProcPtr CloseScreen;

	Bool HWCursor;
	int HWCursorState;
	int defaultVsync;
	int FBCache;

	int drmFD;
	char drmDevName[64];

	int kgsl_3d0_fd;

	/* for now just a single ringbuffer.. not sure if we need more..
	 * probably would like more until context restore works in a sane
	 * way..
	 */
	struct kgsl_ringbuffer *rings[2];

	int pixmapMemtype;
	int DRIMemtype;
	struct msm_drm_bo *cachedBo;

	struct msm_drm_bo *fbBo;
	void *curVisiblePtr;

	OptionInfoPtr     options;
	PixmapPtr rotatedPixmap;
	Bool isFBSurfaceStale;
	PictTransform   *currentTransform;
} MSMRec, *MSMPtr;

struct msm_pixmap_priv {
	struct msm_drm_bo *bo;
	int SavedPitch;
};

/* Macro to get the private record from the ScreenInfo structure */
#define MSMPTR(p) ((MSMPtr) ((p)->driverPrivate))

#define MSMPTR_FROM_PIXMAP(_x)         \
		MSMPTR(xf86Screens[(_x)->drawable.pScreen->myNum])

Bool MSMSetupAccel(ScreenPtr pScreen);
Bool MSMSetupExa(ScreenPtr);
void MSMSetCursorPosition(MSMPtr pMsm, int x, int y);
void MSMCursorEnable(MSMPtr pMsm, Bool enable);
void MSMCursorLoadARGB(MSMPtr pMsm, CARD32 * image);
Bool MSMCursorInit(ScreenPtr pScreen);
void MSMOutputSetup(ScrnInfoPtr pScrn);
void MSMCrtcSetup(ScrnInfoPtr pScrn);

#define MSM_OFFSCREEN_GEM 0x01


#define xFixedtoDouble(_f) (double) ((_f)/(double) xFixed1)

unsigned int msm_pixmap_gpuptr(PixmapPtr);
void *msm_pixmap_hostptr(PixmapPtr);
int msm_pixmap_offset(PixmapPtr);
int msm_pixmap_get_pitch(PixmapPtr pix);
Bool msm_pixmap_in_gem(PixmapPtr);
struct msm_drm_bo *msm_get_pixmap_bo(PixmapPtr);


/**
 * This controls whether debug statements (and function "trace" enter/exit)
 * messages are sent to the log file (TRUE) or are ignored (FALSE).
 */
extern Bool msmDebug;


/* Various logging/debug macros for use in the X driver and the external
 * sub-modules:
 */
#define DEBUG_MSG(fmt, ...) \
		do { if (msmDebug) xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s:%d " fmt "\n",\
				__FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)
#define INFO_MSG(fmt, ...) \
		do { xf86DrvMsg(pScrn->scrnIndex, X_INFO, fmt "\n",\
				##__VA_ARGS__); } while (0)
#define WARNING_MSG(fmt, ...) \
		do { xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "WARNING: " fmt "\n",\
				##__VA_ARGS__); } while (0)
#define ERROR_MSG(fmt, ...) \
		do { xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "ERROR: " fmt "\n",\
				##__VA_ARGS__); } while (0)
#define EARLY_ERROR_MSG(fmt, ...) \
		do { xf86Msg(X_ERROR, "ERROR: " fmt "\n",\
				##__VA_ARGS__); } while (0)

#endif
