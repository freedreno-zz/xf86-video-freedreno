/* msm-exa.c
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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
#include "exa.h"

#include "msm.h"
#include "msm-drm.h"
#include "msm-accel.h"

#define xFixedtoDouble(_f) (double) ((_f)/(double) xFixed1)

#define ENABLE_EXA_TRACE				1
#define ENABLE_SW_FALLBACK_REPORTS		1

#define SCREEN(pDraw) \
	ScrnInfoPtr pScrn = xf86Screens[((DrawablePtr)(pDraw))->pScreen->myNum];

#define TRACE_EXA(fmt, ...) do {									\
		if (ENABLE_EXA_TRACE)										\
			DEBUG_MSG(fmt, ##__VA_ARGS__);							\
	} while (0)

#define EXA_FAIL_IF(cond) do {										\
		if (cond) {													\
			if (ENABLE_SW_FALLBACK_REPORTS) {						\
				DEBUG_MSG("fallback: " #cond);						\
			}														\
			return FALSE;											\
		}															\
	} while (0)


#if 0
/* Get the length of the vector represented by (x,y) */
static inline double
vector_length(double x, double y)
{
	return sqrt((x*x) + (y*y));
}

static inline void
vector_normalize(double *x, double *y)
{
	double len = vector_length(*x, *y);
	if (len != 0.0) {
		*x /= len;
		*y /= len;
	}
}

static void
transform_get_translate(PictTransform *t, double *x, double *y)
{
	double scale = xFixedtoDouble(t->matrix[2][2]);

	if (scale == 0)
		return;

	*x = xFixedtoDouble(t->matrix[0][2]) / scale;
	*y = xFixedtoDouble(t->matrix[1][2]) / scale;
}

static void
transform_get_scale(PictTransform *t, double *xscale, double *yscale)
{
	double scale = xFixedtoDouble(t->matrix[2][2]);
	double xs, ys;

	if (scale == 0)
		return;

	xs = (vector_length(xFixedtoDouble(t->matrix[0][0]) / scale,
			xFixedtoDouble(t->matrix[0][1]) / scale));

	ys = (vector_length(xFixedtoDouble(t->matrix[1][0]) / scale,
			xFixedtoDouble(t->matrix[1][1]) / scale));

	/* The returned value is the inverse of what we calculate, because
       the matrix is mapping the transformed surface back to the source
       surface */

	if (xs)
		xs = 1/xs;

	if (ys)
		ys = 1/ys;

	*xscale = xs;
	*yscale = ys;
}

static void
transform_get_rotate(PictTransform *t, double *d)
{
	double row[2][2];
	double ret;
	double scale = xFixedtoDouble(t->matrix[2][2]);

	if (scale == 0)
		return;

	row[0][0] = xFixedtoDouble(t->matrix[0][0]) / scale;
	row[0][1] = xFixedtoDouble(t->matrix[0][1]) / scale;
	row[1][0] = xFixedtoDouble(t->matrix[1][0]) / scale;
	row[1][1] = xFixedtoDouble(t->matrix[1][1]) / scale;

	vector_normalize(&row[0][0], &row[0][1]);
	vector_normalize(&row[1][0], &row[1][1]);

	ret = atan2(row[0][1], row[0][0]) - atan2(0, 1);

	*d = ret >= 0 ? ret : (2 * M_PI) + ret;
}
#endif

/**
 * PrepareSolid() sets up the driver for doing a solid fill.
 * @param pPixmap Destination pixmap
 * @param alu raster operation
 * @param planemask write mask for the fill
 * @param fg "foreground" color for the fill
 *
 * This call should set up the driver for doing a series of solid fills
 * through the Solid() call.  The alu raster op is one of the GX*
 * graphics functions listed in X.h, and typically maps to a similar
 * single-byte "ROP" setting in all hardware.  The planemask controls
 * which bits of the destination should be affected, and will only represent
 * the bits up to the depth of pPixmap.  The fg is the pixel value of the
 * foreground color referred to in ROP descriptions.
 *
 * Note that many drivers will need to store some of the data in the driver
 * private record, for sending to the hardware with each drawing command.
 *
 * The PrepareSolid() call is required of all drivers, but it may fail for any
 * reason.  Failure results in a fallback to software rendering.
 */
static Bool
MSMPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
	SCREEN(pPixmap);

	TRACE_EXA("%p <- alu=%x, planemask=%08x, fg=%08x",
			pPixmap, alu, (unsigned int)planemask, (unsigned int)fg);

	EXA_FAIL_IF(planemask != FB_ALLONES);
	EXA_FAIL_IF(alu != GXcopy);

	EXA_FAIL_IF(TRUE);

	return TRUE;
}

/**
 * Solid() performs a solid fill set up in the last PrepareSolid() call.
 *
 * @param pPixmap destination pixmap
 * @param x1 left coordinate
 * @param y1 top coordinate
 * @param x2 right coordinate
 * @param y2 bottom coordinate
 *
 * Performs the fill set up by the last PrepareSolid() call, covering the
 * area from (x1,y1) to (x2,y2) in pPixmap.  Note that the coordinates are
 * in the coordinate space of the destination pixmap, so the driver will
 * need to set up the hardware's offset and pitch for the destination
 * coordinates according to the pixmap's offset and pitch within
 * framebuffer.  This likely means using exaGetPixmapOffset() and
 * exaGetPixmapPitch().
 *
 * This call is required if PrepareSolid() ever succeeds.
 */
static void
MSMSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	SCREEN(pPixmap);
	TRACE_EXA("x1=%d\ty1=%d\tx2=%d\ty2=%d", x1, y1, x2, y2);
}

/**
 * DoneSolid() finishes a set of solid fills.
 *
 * @param pPixmap destination pixmap.
 *
 * The DoneSolid() call is called at the end of a series of consecutive
 * Solid() calls following a successful PrepareSolid().  This allows drivers
 * to finish up emitting drawing commands that were buffered, or clean up
 * state from PrepareSolid().
 *
 * This call is required if PrepareSolid() ever succeeds.
 */
static void
MSMDoneSolid(PixmapPtr pPixmap)
{

}

/**
 * PrepareCopy() sets up the driver for doing a copy within video
 * memory.
 *
 * @param pSrcPixmap source pixmap
 * @param pDstPixmap destination pixmap
 * @param dx X copy direction
 * @param dy Y copy direction
 * @param alu raster operation
 * @param planemask write mask for the fill
 *
 * This call should set up the driver for doing a series of copies from the
 * the pSrcPixmap to the pDstPixmap.  The dx flag will be positive if the
 * hardware should do the copy from the left to the right, and dy will be
 * positive if the copy should be done from the top to the bottom.  This
 * is to deal with self-overlapping copies when pSrcPixmap == pDstPixmap.
 * If your hardware can only support blits that are (left to right, top to
 * bottom) or (right to left, bottom to top), then you should set
 * #EXA_TWO_BITBLT_DIRECTIONS, and EXA will break down Copy operations to
 * ones that meet those requirements.  The alu raster op is one of the GX*
 * graphics functions listed in X.h, and typically maps to a similar
 * single-byte "ROP" setting in all hardware.  The planemask controls which
 * bits of the destination should be affected, and will only represent the
 * bits up to the depth of pPixmap.
 *
 * Note that many drivers will need to store some of the data in the driver
 * private record, for sending to the hardware with each drawing command.
 *
 * The PrepareCopy() call is required of all drivers, but it may fail for any
 * reason.  Failure results in a fallback to software rendering.
 */
static Bool
MSMPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx, int dy,
		int alu, Pixel planemask)
{
	SCREEN(pDstPixmap);

	TRACE_EXA("%p <- %p", pDstPixmap, pSrcPixmap);

	EXA_FAIL_IF(planemask != FB_ALLONES);
	EXA_FAIL_IF(alu != GXcopy);

	EXA_FAIL_IF(TRUE);

	return TRUE;
}

/**
 * Copy() performs a copy set up in the last PrepareCopy call.
 *
 * @param pDstPixmap destination pixmap
 * @param srcX source X coordinate
 * @param srcY source Y coordinate
 * @param dstX destination X coordinate
 * @param dstY destination Y coordinate
 * @param width width of the rectangle to be copied
 * @param height height of the rectangle to be copied.
 *
 * Performs the copy set up by the last PrepareCopy() call, copying the
 * rectangle from (srcX, srcY) to (srcX + width, srcY + width) in the source
 * pixmap to the same-sized rectangle at (dstX, dstY) in the destination
 * pixmap.  Those rectangles may overlap in memory, if
 * pSrcPixmap == pDstPixmap.  Note that this call does not receive the
 * pSrcPixmap as an argument -- if it's needed in this function, it should
 * be stored in the driver private during PrepareCopy().  As with Solid(),
 * the coordinates are in the coordinate space of each pixmap, so the driver
 * will need to set up source and destination pitches and offsets from those
 * pixmaps, probably using exaGetPixmapOffset() and exaGetPixmapPitch().
 *
 * This call is required if PrepareCopy ever succeeds.
 */
static void
MSMCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY,
		int width, int height)
{
	SCREEN(pDstPixmap);

	TRACE_EXA("srcX=%d\tsrcY=%d\tdstX=%d\tdstY=%d\twidth=%d\theight=%d",
			srcX, srcY, dstX, dstY, width, height);
}

/**
 * DoneCopy() finishes a set of copies.
 *
 * @param pPixmap destination pixmap.
 *
 * The DoneCopy() call is called at the end of a series of consecutive
 * Copy() calls following a successful PrepareCopy().  This allows drivers
 * to finish up emitting drawing commands that were buffered, or clean up
 * state from PrepareCopy().
 *
 * This call is required if PrepareCopy() ever succeeds.
 */
static void
MSMDoneCopy(PixmapPtr pDstPixmap)
{

}

/**
 * CheckComposite() checks to see if a composite operation could be
 * accelerated.
 *
 * @param op Render operation
 * @param pSrcPicture source Picture
 * @param pMaskPicture mask picture
 * @param pDstPicture destination Picture
 *
 * The CheckComposite() call checks if the driver could handle acceleration
 * of op with the given source, mask, and destination pictures.  This allows
 * drivers to check source and destination formats, supported operations,
 * transformations, and component alpha state, and send operations it can't
 * support to software rendering early on.  This avoids costly pixmap
 * migration to the wrong places when the driver can't accelerate
 * operations.  Note that because migration hasn't happened, the driver
 * can't know during CheckComposite() what the offsets and pitches of the
 * pixmaps are going to be.
 *
 * See PrepareComposite() for more details on likely issues that drivers
 * will have in accelerating Composite operations.
 *
 * The CheckComposite() call is recommended if PrepareComposite() is
 * implemented, but is not required.
 */
static Bool
MSMCheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		PicturePtr pDstPicture)
{
	SCREEN(pDstPicture->pDrawable);
	TRACE_EXA("%p <- %p (%p)", pDstPicture, pSrcPicture, pMaskPicture);
	EXA_FAIL_IF(TRUE);
	return TRUE;
}

/**
 * PrepareComposite() sets up the driver for doing a Composite operation
 * described in the Render extension protocol spec.
 *
 * @param op Render operation
 * @param pSrcPicture source Picture
 * @param pMaskPicture mask picture
 * @param pDstPicture destination Picture
 * @param pSrc source pixmap
 * @param pMask mask pixmap
 * @param pDst destination pixmap
 *
 * This call should set up the driver for doing a series of Composite
 * operations, as described in the Render protocol spec, with the given
 * pSrcPicture, pMaskPicture, and pDstPicture.  The pSrc, pMask, and
 * pDst are the pixmaps containing the pixel data, and should be used for
 * setting the offset and pitch used for the coordinate spaces for each of
 * the Pictures.
 *
 * Notes on interpreting Picture structures:
 * - The Picture structures will always have a valid pDrawable.
 * - The Picture structures will never have alphaMap set.
 * - The mask Picture (and therefore pMask) may be NULL, in which case the
 *   operation is simply src OP dst instead of src IN mask OP dst, and
 *   mask coordinates should be ignored.
 * - pMarkPicture may have componentAlpha set, which greatly changes
 *   the behavior of the Composite operation.  componentAlpha has no effect
 *   when set on pSrcPicture or pDstPicture.
 * - The source and mask Pictures may have a transformation set
 *   (Picture->transform != NULL), which means that the source coordinates
 *   should be transformed by that transformation, resulting in scaling,
 *   rotation, etc.  The PictureTransformPoint() call can transform
 *   coordinates for you.  Transforms have no effect on Pictures when used
 *   as a destination.
 * - The source and mask pictures may have a filter set.  PictFilterNearest
 *   and PictFilterBilinear are defined in the Render protocol, but others
 *   may be encountered, and must be handled correctly (usually by
 *   PrepareComposite failing, and falling back to software).  Filters have
 *   no effect on Pictures when used as a destination.
 * - The source and mask Pictures may have repeating set, which must be
 *   respected.  Many chipsets will be unable to support repeating on
 *   pixmaps that have a width or height that is not a power of two.
 *
 * If your hardware can't support source pictures (textures) with
 * non-power-of-two pitches, you should set #EXA_OFFSCREEN_ALIGN_POT.
 *
 * Note that many drivers will need to store some of the data in the driver
 * private record, for sending to the hardware with each drawing command.
 *
 * The PrepareComposite() call is not required.  However, it is highly
 * recommended for performance of antialiased font rendering and performance
 * of cairo applications.  Failure results in a fallback to software
 * rendering.
 */
static Bool
MSMPrepareComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		PicturePtr pDstPicture, PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
	SCREEN(pDst);
	TRACE_EXA("%p <- %p (%p)", pDst, pSrc, pMask);
	EXA_FAIL_IF(TRUE);
	return TRUE;
}

/**
 * Composite() performs a Composite operation set up in the last
 * PrepareComposite() call.
 *
 * @param pDstPixmap destination pixmap
 * @param srcX source X coordinate
 * @param srcY source Y coordinate
 * @param maskX source X coordinate
 * @param maskY source Y coordinate
 * @param dstX destination X coordinate
 * @param dstY destination Y coordinate
 * @param width destination rectangle width
 * @param height destination rectangle height
 *
 * Performs the Composite operation set up by the last PrepareComposite()
 * call, to the rectangle from (dstX, dstY) to (dstX + width, dstY + height)
 * in the destination Pixmap.  Note that if a transformation was set on
 * the source or mask Pictures, the source rectangles may not be the same
 * size as the destination rectangles and filtering.  Getting the coordinate
 * transformation right at the subpixel level can be tricky, and rendercheck
 * can test this for you.
 *
 * This call is required if PrepareComposite() ever succeeds.
 */
static void
MSMComposite(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY,
		int dstX, int dstY, int width, int height)
{
	SCREEN(pDst);
	TRACE_EXA("srcX=%d\tsrcY=%d\tmaskX=%d\tmaskY=%d\tdstX=%d\tdstY=%d\twidth=%d\theight=%d",
			srcX, srcY, maskX, maskY, dstX, dstY, width, height);
}

/**
 * DoneComposite() finishes a set of Composite operations.
 *
 * @param pPixmap destination pixmap.
 *
 * The DoneComposite() call is called at the end of a series of consecutive
 * Composite() calls following a successful PrepareComposite().  This allows
 * drivers to finish up emitting drawing commands that were buffered, or
 * clean up state from PrepareComposite().
 *
 * This call is required if PrepareComposite() ever succeeds.
 */
static void
MSMDoneComposite(PixmapPtr pDst)
{

}

/**
 * MarkSync() requests that the driver mark a synchronization point,
 * returning an driver-defined integer marker which could be requested for
 * synchronization to later in WaitMarker().  This might be used in the
 * future to avoid waiting for full hardware stalls before accessing pixmap
 * data with the CPU, but is not important in the current incarnation of
 * EXA.
 *
 * Note that drivers should call exaMarkSync() when they have done some
 * acceleration, rather than their own MarkSync() handler, as otherwise EXA
 * will be unaware of the driver's acceleration and not sync to it during
 * fallbacks.
 *
 * MarkSync() is optional.
 */
static int
MSMMarkSync(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);
	return kgsl_ringbuffer_mark(pMsm->rings[1]);
}


/**
 * WaitMarker() waits for all rendering before the given marker to have
 * completed.  If the driver does not implement MarkSync(), marker is
 * meaningless, and all rendering by the hardware should be completed before
 * WaitMarker() returns.
 *
 * Note that drivers should call exaWaitSync() to wait for all acceleration
 * to finish, as otherwise EXA will be unaware of the driver having
 * synchronized, resulting in excessive WaitMarker() calls.
 *
 * WaitMarker() is required of all drivers.
 */
static void
MSMWaitMarker(ScreenPtr pScreen, int marker)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);
	kgsl_ringbuffer_wait(pMsm->rings[1], marker);
}

static Bool
MSMPixmapIsOffscreen(PixmapPtr pPixmap)
{
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	MSMPtr pMsm = MSMPTR_FROM_PIXMAP(pPixmap);

	struct msm_pixmap_priv *priv;
	if ((pScreen->GetScreenPixmap(pScreen) == pPixmap) ||
			(pMsm->rotatedPixmap == pPixmap)){
		return TRUE;
	}

	priv = exaGetPixmapDriverPrivate(pPixmap);

	if (priv && priv->bo)
		return TRUE;

	return FALSE;
}

static Bool
MSMPrepareAccess(PixmapPtr pPixmap, int index)
{
	struct msm_pixmap_priv *priv;

	priv = exaGetPixmapDriverPrivate(pPixmap);

	if (!priv)
		return FALSE;

	if (!priv->bo)
		return TRUE;

	if (msm_drm_bo_map(priv->bo))
		return FALSE;

	if (pPixmap->devPrivate.ptr == NULL)
		pPixmap->devPrivate.ptr = (void *) priv->bo->hostptr;

	if (pPixmap->drawable.bitsPerPixel == 16 ||
			pPixmap->drawable.bitsPerPixel == 32) {
		priv->SavedPitch = pPixmap->devKind;

		pPixmap->devKind = ((pPixmap->drawable.width + 31) & ~31) *
				(pPixmap->drawable.bitsPerPixel >> 3);
	}

	return TRUE;
}

static void
MSMFinishAccess(PixmapPtr pPixmap, int index)
{
	struct msm_pixmap_priv *priv;
	priv = exaGetPixmapDriverPrivate(pPixmap);

	if (!priv || !priv->bo)
		return;

	if (priv->SavedPitch) {
		pPixmap->devKind = priv->SavedPitch;
		priv->SavedPitch = 0;
	}

	pPixmap->devPrivate.ptr = NULL;
}

static void *
MSMCreatePixmap(ScreenPtr pScreen, int size, int align)
{
	struct msm_pixmap_priv *priv;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);

	priv = calloc(1, sizeof(struct msm_pixmap_priv));

	if (priv == NULL)
		return NULL;

	if (!size)
		return priv;

	priv->bo = msm_drm_bo_create(pMsm, size, pMsm->pixmapMemtype);

	if (priv->bo)
		return priv;

	free(priv);
	return NULL;
}

static void
MSMDestroyPixmap(ScreenPtr pScreen, void *dpriv)
{
	struct msm_pixmap_priv *priv = dpriv;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);

	if (!dpriv)
		return;

	if (priv->bo)
		msm_drm_bo_free(pMsm, priv->bo);

	free(dpriv);
}

Bool
MSMSetupExa(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);
	ExaDriverPtr pExa;

	/* Set up EXA */
	xf86LoadSubModule(pScrn, "exa");

	if (pMsm->pExa == NULL)
		pMsm->pExa = exaDriverAlloc();

	if (pMsm->pExa == NULL)
		return FALSE;

	pExa = pMsm->pExa;

	/* This is the current major/minor that we support */

	pExa->exa_major = 2;
	pExa->exa_minor = 2;

	pExa->memoryBase = pMsm->fbmem;

	/* Max blit extents that hw supports */
	pExa->maxX = 2048;
	pExa->maxY = 2048;

	pExa->flags = EXA_OFFSCREEN_PIXMAPS | EXA_HANDLES_PIXMAPS | EXA_SUPPORTS_PREPARE_AUX;

	pExa->offScreenBase =
			(pMsm->fixed_info.line_length * pMsm->mode_info.yres);
	pExa->memorySize = pMsm->fixed_info.smem_len;

	/* Align pixmap offsets along page boundaries */
	pExa->pixmapOffsetAlign = 4096;

	/* Align pixmap pitches to the maximum needed aligment for the
      GPU - this ensures that we have enough room, and we adjust the
      pitches down to the depth later */

	pExa->pixmapPitchAlign = 128;

	/* The maximum acceleratable pitch is 2048 pixels */
	pExa->maxPitchPixels = 2048;

	pExa->PrepareSolid       = MSMPrepareSolid;
	pExa->Solid              = MSMSolid;
	pExa->DoneSolid          = MSMDoneSolid;
	pExa->PrepareCopy        = MSMPrepareCopy;
	pExa->Copy               = MSMCopy;
	pExa->DoneCopy           = MSMDoneCopy;
	pExa->CheckComposite     = MSMCheckComposite;
	pExa->PrepareComposite   = MSMPrepareComposite;
	pExa->Composite          = MSMComposite;
	pExa->DoneComposite      = MSMDoneComposite;
	pExa->MarkSync           = MSMMarkSync;
	pExa->WaitMarker         = MSMWaitMarker;
	pExa->PixmapIsOffscreen  = MSMPixmapIsOffscreen;
	pExa->CreatePixmap       = MSMCreatePixmap;
	pExa->DestroyPixmap      = MSMDestroyPixmap;
	pExa->PrepareAccess      = MSMPrepareAccess;
	pExa->FinishAccess       = MSMFinishAccess;

	return exaDriverInit(pScreen, pMsm->pExa);
}
