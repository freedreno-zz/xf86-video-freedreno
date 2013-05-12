/*
 * Copyright © 2012 Rob Clark <robclark@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "msm.h"

#include "xf86drmMode.h"
#include "dri2.h"

typedef struct {
	DRI2BufferRec base;

	/**
	 * Pixmap that is backing the buffer
	 *
	 * NOTE: don't track the pixmap ptr for the front buffer if it is
	 * a window.. this could get reallocated from beneath us, so we should
	 * always use draw2pix to be sure to have the correct one
	 */
	PixmapPtr pPixmap;

} MSMDRI2BufferRec, *MSMDRI2BufferPtr;

#define MSMBUF(p)	((MSMDRI2BufferPtr)(p))
#define DRIBUF(p)	((DRI2BufferPtr)(&(p)->base))

static void MSMDRI2DestroyBuffer(DrawablePtr pDraw, DRI2BufferPtr buffer);

static inline DrawablePtr
dri2draw(DrawablePtr pDraw, DRI2BufferPtr buf)
{
	if (buf->attachment == DRI2BufferFrontLeft) {
		return pDraw;
	} else {
		return &(MSMBUF(buf)->pPixmap->drawable);
	}
}

static inline PixmapPtr
draw2pix(DrawablePtr pDraw)
{
	if (!pDraw) {
		return NULL;
	} else if (pDraw->type == DRAWABLE_WINDOW) {
		return pDraw->pScreen->GetWindowPixmap((WindowPtr)pDraw);
	} else {
		return (PixmapPtr)pDraw;
	}
}

static PixmapPtr
createpix(DrawablePtr pDraw)
{
	ScreenPtr pScreen = pDraw->pScreen;
	return pScreen->CreatePixmap(pScreen,
			pDraw->width, pDraw->height, pDraw->depth,
			CREATE_PIXMAP_USAGE_DRI2);
}

/**
 * Create Buffer.
 */
static DRI2BufferPtr
MSMDRI2CreateBuffer(DrawablePtr pDraw, unsigned int attachment,
		unsigned int format)
{
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	MSMDRI2BufferPtr buf = calloc(1, sizeof(*buf));
	PixmapPtr pPixmap;
	int ret;

	DEBUG_MSG("pDraw=%p, attachment=%d, format=%08x",
			pDraw, attachment, format);

	if (!buf) {
		return NULL;
	}

	if (attachment == DRI2BufferFrontLeft) {
		pPixmap = draw2pix(pDraw);
		pPixmap->refcnt++;
	} else {
		pPixmap = createpix(pDraw);
	}

	DRIBUF(buf)->attachment = attachment;
	DRIBUF(buf)->cpp = pPixmap->drawable.bitsPerPixel / 8;
	DRIBUF(buf)->format = format;
	buf->pPixmap = pPixmap;

	ret = msm_get_pixmap_name(pPixmap, &DRIBUF(buf)->name,
			&DRIBUF(buf)->pitch);
	if (ret) {
		ERROR_MSG("could not get buffer name: %d", ret);
		MSMDRI2DestroyBuffer(pDraw, DRIBUF(buf));
		return NULL;
	}

	return DRIBUF(buf);
}

/**
 * Destroy Buffer
 */
static void
MSMDRI2DestroyBuffer(DrawablePtr pDraw, DRI2BufferPtr buffer)
{
	MSMDRI2BufferPtr buf = MSMBUF(buffer);
	ScreenPtr pScreen = buf->pPixmap->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);

	DEBUG_MSG("pDraw=%p, buffer=%p", pDraw, buffer);

	pScreen->DestroyPixmap(buf->pPixmap);

	free(buf);
}

/**
 *
 */
static void
MSMDRI2CopyRegion(DrawablePtr pDraw, RegionPtr pRegion,
		DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	DrawablePtr pSrcDraw = dri2draw(pDraw, pSrcBuffer);
	DrawablePtr pDstDraw = dri2draw(pDraw, pDstBuffer);
	RegionPtr pCopyClip;
	GCPtr pGC;

	DEBUG_MSG("pDraw=%p, pDstBuffer=%p (%p), pSrcBuffer=%p (%p)",
			pDraw, pDstBuffer, pSrcDraw, pSrcBuffer, pDstDraw);

	/* hack.. since we don't have proper fencing / kernel synchronization
	 * we can get in a scenario where we get many frames ahead of the gpu,
	 * with queued up cmd sequence like: render -> blit -> render -> blit ..
	 * This hack makes sure the previous blit has completed.
	 */
	{
	MSMPtr pMsm = MSMPTR(pScrn);
	MSMDRI2BufferPtr buf = MSMBUF(pDstBuffer);
	pMsm->pExa->PrepareAccess(buf->pPixmap, 0);
	pMsm->pExa->FinishAccess(buf->pPixmap, 0);
	}

	pGC = GetScratchGC(pDstDraw->depth, pScreen);
	if (!pGC) {
		return;
	}

	pCopyClip = REGION_CREATE(pScreen, NULL, 0);
	RegionCopy(pCopyClip, pRegion);
	(*pGC->funcs->ChangeClip) (pGC, CT_REGION, pCopyClip, 0);
	ValidateGC(pDstDraw, pGC);

	/* If the dst is the framebuffer, and we had a way to
	 * schedule a deferred blit synchronized w/ vsync, that
	 * would be a nice thing to do utilize here to avoid
	 * tearing..  when we have sync object support for GEM
	 * buffers, I think we could do something more clever
	 * here.
	 */

	pGC->ops->CopyArea(pSrcDraw, pDstDraw, pGC,
			0, 0, pDraw->width, pDraw->height, 0, 0);

	FreeScratchGC(pGC);

	MSMFlushAccel(pScreen);
}

/**
 * The DRI2 ScreenInit() function.. register our handler fxns w/ DRI2 core
 */
Bool
MSMDRI2ScreenInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	MSMPtr pMsm = MSMPTR(pScrn);
	DRI2InfoRec info = {
			.version			= 3,
			.fd 				= pMsm->drmFD,
			.driverName			= "kgsl",
			.deviceName			= pMsm->deviceName,
			.CreateBuffer		= MSMDRI2CreateBuffer,
			.DestroyBuffer		= MSMDRI2DestroyBuffer,
			.CopyRegion			= MSMDRI2CopyRegion,
			.AuthMagic			= drmAuthMagic,
	};
	int minor = 1, major = 0;

	if (xf86LoaderCheckSymbol("DRI2Version")) {
		DRI2Version(&major, &minor);
	}

	if (minor < 1) {
		WARNING_MSG("DRI2 requires DRI2 module version 1.1.0 or later");
		return FALSE;
	}

	return DRI2ScreenInit(pScreen, &info);
}

/**
 * The DRI2 CloseScreen() function.. unregister ourself w/ DRI2 core.
 */
void
MSMDRI2CloseScreen(ScreenPtr pScreen)
{
	DRI2CloseScreen(pScreen);
}
