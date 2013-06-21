/* msm-output.c
k *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif

#include <sys/ioctl.h>
#include "xf86.h"
#include "xf86Crtc.h"
#include "xf86_OSlib.h"
#include "msm.h"


static void
MSMCrtcGammaSet(xf86CrtcPtr crtc,
		CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	/* This is a new function that gets called by the DI code  */

}

static void
MSMCrtcDPMS(xf86CrtcPtr crtc, int mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	MSMPtr pMsm = MSMPTR(pScrn);
	int ret = ioctl(pMsm->fd, FBIOBLANK, mode);
	if (ret)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unable to set dpms: %m\n");
}

static Bool
MSMCrtcLock(xf86CrtcPtr crtc)
{
	return TRUE;
}

static void
MSMCrtcUnlock(xf86CrtcPtr crtc)
{
}

static void
MSMCrtcPrepare(xf86CrtcPtr crtc)
{
	/* Blank the display before we change modes? */
}

static Bool
MSMCrtcModeFixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		DisplayModePtr adjmode)
{
	ScrnInfoPtr pScrn = crtc->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	if (mode->HDisplay > pMsm->mode_info.xres_virtual ||
			mode->VDisplay > pMsm->mode_info.yres_virtual)
		return FALSE;

	return TRUE;
}

static void
MSMCrtcModeSet(xf86CrtcPtr crtc, DisplayModePtr mode,
		DisplayModePtr adjmode, int x, int y)
{
	int ret;
	ScrnInfoPtr pScrn = crtc->scrn;
	MSMPtr pMsm = MSMPTR(pScrn);
	struct fb_var_screeninfo var;
	int htotal, vtotal;

	memcpy(&var, &pMsm->mode_info, sizeof(var));

	htotal = var.xres + var.right_margin + var.hsync_len + var.left_margin;

	var.xres = adjmode->HDisplay;
	var.right_margin = adjmode->HSyncStart - adjmode->HDisplay;
	var.hsync_len = adjmode->HSyncEnd - adjmode->HSyncStart;
	var.left_margin = adjmode->HTotal - adjmode->HSyncEnd;

	vtotal = var.yres + var.lower_margin + var.vsync_len + var.upper_margin;

	var.yres = adjmode->VDisplay;
	var.lower_margin = adjmode->VSyncStart - adjmode->VDisplay;
	var.vsync_len = adjmode->VSyncEnd - adjmode->VSyncStart;
	var.upper_margin = adjmode->VTotal - adjmode->VSyncEnd;

	if (vtotal != adjmode->VTotal || htotal != adjmode->HTotal)
		var.pixclock = pMsm->defaultVsync * adjmode->HTotal * adjmode->VTotal;
	/*crtc->rotatedData!= NULL indicates that rotation has been requested
   and shadow framebuffer has been allocated, so change the yoffset to make
   the shadow framebuffer as visible screen. */
	var.yoffset = (crtc->rotatedData && crtc->rotation != 1) ? var.yres : 0;

	ret = ioctl(pMsm->fd, FBIOPUT_VSCREENINFO, &var);

	if (ret)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unable to change the mode: %m\n");
	else {
		/* Refresh the changed settings from the driver */
		if (crtc->scrn->pScreen)
			xf86_reload_cursors(crtc->scrn->pScreen);
		ioctl(pMsm->fd, FBIOGET_VSCREENINFO, &pMsm->mode_info);
	}
}

static void
MSMCrtcCommit(xf86CrtcPtr crtc)
{
	MSMCrtcDPMS(crtc, DPMSModeOn);
}

static void *
MSMCrtcShadowAllocate(xf86CrtcPtr crtc, int width, int height)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	MSMPtr pMsm = MSMPTR(pScrn);
	/* (pMsm->fixed_info.line_length * pMsm->mode_info.yres) is the size of
    original framebuufer. As buffer is already preallocated by kernel, so just
    return the memory address after the end of original framebuffer as the
    starting address of the shadow framebuffer.*/
	memset((char*)(pMsm->fbmem + pMsm->mode_info.yres *
			pMsm->fixed_info.line_length), 0, pMsm->mode_info.yres *
			pMsm->fixed_info.line_length );
	return (pMsm->fbmem + pMsm->mode_info.yres * pMsm->fixed_info.line_length);
}

static PixmapPtr
MSMCrtcShadowCreate(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	PixmapPtr pNewPixmap = NULL;
	MSMPtr pMsm = MSMPTR(pScrn);
	if (!data)
		data = MSMCrtcShadowAllocate(crtc, width, height);
	/*The pitch, width and size of the rotated pixmap has to be the same as
    those of the display framebuffer*/
	pNewPixmap = GetScratchPixmapHeader(pScrn->pScreen,pMsm->mode_info.xres,
			pMsm->mode_info.yres,pScrn->depth, pScrn->bitsPerPixel,
			pMsm->fixed_info.line_length, data);
	if (!pNewPixmap)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Unable to allocate shadow pixmap for rotation\n");
	pMsm->rotatedPixmap = pNewPixmap;
	return pNewPixmap;
}

static void
MSMCrtcShadowDestroy(xf86CrtcPtr crtc, PixmapPtr pPixmap, void *data)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	MSMPtr pMsm = MSMPTR(pScrn);
	pMsm->rotatedPixmap = NULL;
	if (pPixmap)
		FreeScratchPixmapHeader(pPixmap);
}

static void
MSMCrtcSetCursorPosition(xf86CrtcPtr crtc, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	MSMSetCursorPosition(pMsm, x, y);
}

static void
MSMCrtcShowCursor(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	MSMCursorEnable(pMsm, TRUE);
}

static void
MSMCrtcHideCursor(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	MSMCursorEnable(pMsm, FALSE);
}

static void
MSMCrtcLoadCursorARGB(xf86CrtcPtr crtc, CARD32 * image)
{
	ScrnInfoPtr pScrn = crtc->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	MSMCursorLoadARGB(pMsm, image);
}

static const xf86CrtcFuncsRec MSMCrtcFuncs = {
		.dpms = MSMCrtcDPMS,
		.lock = MSMCrtcLock,
		.unlock = MSMCrtcUnlock,
		.mode_fixup = MSMCrtcModeFixup,
		.prepare = MSMCrtcPrepare,
		.mode_set = MSMCrtcModeSet,
		.commit = MSMCrtcCommit,
		.shadow_create = MSMCrtcShadowCreate,
		.shadow_allocate = MSMCrtcShadowAllocate,
		.shadow_destroy = MSMCrtcShadowDestroy,
		.set_cursor_position = MSMCrtcSetCursorPosition,
		.show_cursor = MSMCrtcShowCursor,
		.hide_cursor = MSMCrtcHideCursor,
		.load_cursor_argb = MSMCrtcLoadCursorARGB,
		.gamma_set = MSMCrtcGammaSet,
		.destroy = NULL, /* XXX */
};

void
MSMCrtcSetup(ScrnInfoPtr pScrn)
{
	xf86CrtcPtr crtc = xf86CrtcCreate(pScrn, &MSMCrtcFuncs);
	crtc->driver_private = NULL;
}
