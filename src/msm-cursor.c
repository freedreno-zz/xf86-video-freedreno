/* msm-cursor.c
 *
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

#include "xf86.h"
#include "xf86Crtc.h"
#include "xf86Cursor.h"

#include <sys/ioctl.h>
#include <errno.h>
#include <linux/msm_mdp.h>

#include "msm.h"


#define MSM_CURSOR_WIDTH 64
#define MSM_CURSOR_HEIGHT 64

#ifdef MSMFB_CURSOR

static void
_init_cursor(MSMPtr pMsm, struct fb_cursor *cursor)
{
	memset(cursor, 0, sizeof(*cursor));

	/* This is a workaround for a buggy kernel */

	cursor->image.width = MSM_CURSOR_WIDTH;
	cursor->image.height = MSM_CURSOR_HEIGHT;
	cursor->image.depth = 32;

	cursor->enable = pMsm->HWCursorState;
}

void
MSMSetCursorPosition(MSMPtr pMsm, int x, int y)
{
	struct fb_cursor cursor;

	_init_cursor(pMsm, &cursor);

	if (x < 0)
		x = 0;

	if (y < 0)
		y = 0;

	cursor.set |= FB_CUR_SETPOS;
	cursor.image.dx = x;
	cursor.image.dy = y;

	if (ioctl(pMsm->fd, MSMFB_CURSOR, &cursor))
		ErrorF("%s: Error calling MSMBF_CURSOR\n", __FUNCTION__);
}

void
MSMCursorEnable(MSMPtr pMsm, Bool enable)
{
	struct fb_cursor cursor;

	_init_cursor(pMsm, &cursor);

	pMsm->HWCursorState = cursor.enable = (enable == TRUE) ? 1 : 0;

	if (ioctl(pMsm->fd, MSMFB_CURSOR, &cursor))
		ErrorF("%s: Error calling MSMBF_CURSOR\n", __FUNCTION__);
}

void
MSMCursorLoadARGB(MSMPtr pMsm, CARD32 * image)
{
	struct fb_cursor cursor;

	_init_cursor(pMsm, &cursor);

	cursor.set |= FB_CUR_SETIMAGE;
	cursor.image.data = (char *)image;

	/* BLEND_TRANSP_EN off */
	cursor.image.bg_color = 0xFFFFFFFF;

	/* Per pixel alpha on */
	cursor.image.fg_color = 0;

	if (ioctl(pMsm->fd, MSMFB_CURSOR, &cursor))
		ErrorF("%s: Error calling MSMBF_CURSOR\n", __FUNCTION__);
}

Bool
MSMCursorInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);

	MSMPtr pMsm = MSMPTR(pScrn);

	struct fb_cursor cursor;

	_init_cursor(pMsm, &cursor);

	/* Try to turn off the cursor - if this fails then we don't have
	 * HW cursor support */

	cursor.enable = 0;

	if (ioctl(pMsm->fd, MSMFB_CURSOR, &cursor)) {
		xf86DrvMsg(pScreen->myNum, X_ERROR,
				"Unable to enable the HW cursor: %s\n", strerror(errno));

		return FALSE;
	}

	/* HWCursor is on the air, but not visible (yet) */
	pMsm->HWCursorState = 0;

	return xf86_cursors_init(pScreen, MSM_CURSOR_WIDTH, MSM_CURSOR_HEIGHT,
			HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
			HARDWARE_CURSOR_INVERT_MASK |
			HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
			HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
			HARDWARE_CURSOR_ARGB);
}

#else

/* if MSMFB_CURSOR isn't defined, then this is an older version of the kernel
   that doesn't support it - so just provide some dummy stuff here */

void
MSMCrtcSetCursorPosition(MSMPtr pMsm, int x, int y)
{
}

void
MSMCursorEnable(MSMPtr pMsm, Bool enable)
{
}

void
MSMCursorLoadARGB(MSMPtr pMsm, CARD32 * image)
{
}

Bool
MSMCursorInit(ScreenPtr pScreen)
{
	return FALSE;
}

#endif
