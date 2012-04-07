/* msm-exa.c
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Copyright © 2012 Rob Clark <robclark@freedesktop.org>
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

#define MSM_LOCALS(pDraw) \
	ScrnInfoPtr pScrn = xf86Screens[((DrawablePtr)(pDraw))->pScreen->myNum]; \
	MSMPtr pMsm = MSMPTR(pScrn);									\
	struct kgsl_ringbuffer *ring = pMsm->rings[1]; (void)ring;		\
	struct exa_state *exa = pMsm->exa; (void)exa

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

struct exa_state {
	/* solid state: */
	uint32_t fill;

	/* copy/composite state: */
	const uint32_t *op_dwords;
	PixmapPtr src, mask;
	PicturePtr dstpic, srcpic, maskpic;
};

/* NOTE ARGB and A8 seem to be treated the same when it comes to the
 * composite-op dwords:
 */
static const uint32_t composite_op_dwords[4][PictOpAdd+1][4] = {
	{ /* xRGB->xRGB */
		[PictOpSrc]          = { 0x7c000114, 0x10002010, 0x00000000, 0x18012210 },
		[PictOpIn]           = { 0x7c000114, 0xb0100004, 0x00000000, 0x18110a04 },
		[PictOpOut]          = { 0x7c000114, 0xb0102004, 0x00000000, 0x18112a04 },
		[PictOpOver]         = { 0x7c000114, 0xd080a004, 0x7c000118, 0x8081aa04 },
		[PictOpOutReverse]   = { 0x7c000114, 0x80808040, 0x7c000118, 0x80808840 },
		[PictOpAdd]          = { 0x7c000114, 0x5080a004, 0x7c000118, 0x20818204 },
		[PictOpOverReverse]  = { 0x7c000114, 0x7090a004, 0x7c000118, 0x2091a204 },
		[PictOpInReverse]    = { 0x7c000114, 0x80800040, 0x7c000118, 0x80800840 },
		[PictOpAtop]         = { 0x7c000114, 0xf0908004, 0x7c000118, 0xa0918a04 },
		[PictOpAtopReverse]  = { 0x7c000114, 0xf0902004, 0x7c000118, 0xa0912a04 },
		[PictOpXor]          = { 0x7c000114, 0xf090a004, 0x7c000118, 0xa091aa04 },
	},
	{ /* xRGB->ARGB, xRGB->A8 */
		[PictOpSrc]          = { 0x7c000114, 0x10002010, 0x00000000, 0x18012210 },
		[PictOpIn]           = { 0x7c000114, 0x90100004, 0x00000000, 0x18110a04 },
		[PictOpOut]          = { 0x7c000114, 0x90102004, 0x00000000, 0x18112a04 },
		[PictOpOver]         = { 0x7c000114, 0x9080a004, 0x7c000118, 0x8081aa04 },
		[PictOpOutReverse]   = { 0x7c000114, 0x80808040, 0x7c000118, 0x80808840 },
		[PictOpAdd]          = { 0x7c000114, 0x1080a004, 0x7c000118, 0x20818204 },
		[PictOpOverReverse]  = { 0x7c000114, 0x1090a004, 0x00000000, 0x1891a204 },
		[PictOpInReverse]    = { 0x7c000114, 0x80800040, 0x7c000118, 0x80800840 },
		[PictOpAtop]         = { 0x7c000114, 0x90908004, 0x7c000118, 0x80918a04 },
		[PictOpAtopReverse]  = { 0x7c000114, 0x90902004, 0x7c000118, 0x80912a04 },
		[PictOpXor]          = { 0x7c000114, 0x9090a004, 0x7c000118, 0x8091aa04 },
	},
	{ /* ARGB->xRGB, A8->xRGB */
		[PictOpSrc]          = { 0x00000000, 0x14012010, 0x00000000, 0x18012210 },
		[PictOpIn]           = { 0x7c000114, 0x20110004, 0x00000000, 0x18110a04 },
		[PictOpOut]          = { 0x7c000114, 0x20112004, 0x00000000, 0x18112a04 },
		[PictOpOver]         = { 0x7c000114, 0x4281a004, 0x7c000118, 0x0281aa04 },
		[PictOpOutReverse]   = { 0x7c000114, 0x02808040, 0x7c000118, 0x02808840 },
		[PictOpAdd]          = { 0x7c000114, 0x4081a004, 0x00000000, 0x18898204 },
		[PictOpOverReverse]  = { 0x7c000114, 0x6091a004, 0x7c000118, 0x2091a204 },
		[PictOpInReverse]    = { 0x7c000114, 0x02800040, 0x7c000118, 0x02800840 },
		[PictOpAtop]         = { 0x7c000114, 0x62918004, 0x7c000118, 0x22918a04 },
		[PictOpAtopReverse]  = { 0x7c000114, 0x62912004, 0x7c000118, 0x22912a04 },
		[PictOpXor]          = { 0x7c000114, 0x6291a004, 0x7c000118, 0x2291aa04 },
	},
	{ /* ARGB->ARGB, A8->A8 */
		[PictOpSrc]          = { 0x00000000, 0x14012010, 0x00000000, 0x18012210 },
		[PictOpIn]           = { 0x00000000, 0x14110004, 0x00000000, 0x18110a04 },
		[PictOpOut]          = { 0x00000000, 0x14112004, 0x00000000, 0x18112a04 },
		[PictOpOver]         = { 0x7c000114, 0x0281a004, 0x7c000118, 0x0281aa04 },
		[PictOpOutReverse]   = { 0x7c000114, 0x02808040, 0x7c000118, 0x02808840 },
		[PictOpAdd]          = { 0x00000000, 0x1481a004, 0x00000000, 0x18898204 },
		[PictOpOverReverse]  = { 0x00000000, 0x1491a004, 0x00000000, 0x1891a204 },
		[PictOpInReverse]    = { 0x7c000114, 0x02800040, 0x7c000118, 0x02800840 },
		[PictOpAtop]         = { 0x7c000114, 0x02918004, 0x7c000118, 0x02918a04 },
		[PictOpAtopReverse]  = { 0x7c000114, 0x02912004, 0x7c000118, 0x02912a04 },
		[PictOpXor]          = { 0x7c000114, 0x0291a004, 0x7c000118, 0x0291aa04 },
	},
};

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
	MSM_LOCALS(pPixmap);

	TRACE_EXA("%p <- alu=%x, planemask=%08x, fg=%08x",
			pPixmap, alu, (unsigned int)planemask, (unsigned int)fg);

	EXA_FAIL_IF(planemask != FB_ALLONES);
	EXA_FAIL_IF(alu != GXcopy);

	// TODO other color formats
	EXA_FAIL_IF(pPixmap->drawable.bitsPerPixel != 32);

	exa->fill = fg;

	/* Note: 16bpp 565 we want something like this.. I think..

		color  = ((fg << 3) & 0xf8) | ((fg >> 2) & 0x07) |
			((fg << 5) & 0xfc00)    | ((fg >> 1) & 0x300) |
			((fg << 8) & 0xf80000)  | ((fg << 3) & 0x70000) |
			0xff000000; // implicitly DISABLE_ALPHA

	 */

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
	MSM_LOCALS(pPixmap);
	struct msm_drm_bo *dst_bo = msm_get_pixmap_bo(pPixmap);
	uint32_t w, h, p;

	w = pPixmap->drawable.width;
	h = pPixmap->drawable.height;

	/* pitch specified in units of 32 bytes, it appears.. not quite sure
	 * max size yet, but I think 11 or 12 bits..
	 */
	p = (msm_pixmap_get_pitch(pPixmap) / 32) & 0xfff;

	TRACE_EXA("x1=%d\ty1=%d\tx2=%d\ty2=%d", x1, y1, x2, y2);

	BEGIN_RING(ring, 23);
	OUT_RING  (ring, 0x0c000000);
	OUT_RING  (ring, 0x11000000);
	OUT_RING  (ring, 0xd0030000);
	OUT_RING  (ring, 0xd2000000 | (((h * 2) & 0xfff) << 12) | (w & 0xfff));   // [1]
	OUT_RING  (ring, 0x01007000 | p);
	OUT_RING  (ring, 0x7c000100);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d3);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d1);
	OUT_RING  (ring, 0x40007000 | p);
	OUT_RING  (ring, 0xd5000000);
	OUT_RING  (ring, 0x08000000 | ((x2 & 0xfff) << 12) | (x1 & 0xfff));
	OUT_RING  (ring, 0x09000000 | ((y2 & 0xfff) << 12) | (y1 & 0xfff));
	OUT_RING  (ring, 0x0f000000);                                             // [2]
	OUT_RING  (ring, 0x0f000000);                                             // [2]
	OUT_RING  (ring, 0x0f000001);                                             // [2]
	OUT_RING  (ring, 0x0e000000);
#if 0
	/* there seem to be 3 ways to encode bx/by/bw/bh depending on which
	 * are greater than 0xff.. seems like we can ignore this and always
	 * encode worst case..
	 */
	if ((x1 > 0xff) || (y1 > 0xff)) {
		OUT_RING(ring, 0x7c0002f0);
		OUT_RING(ring, ((x1 & 0xffff) << 16) | (y1 & 0xffff));
		OUT_RING(ring, (((x2 - x1) & 0xffff) << 16) | ((y2 - y1) & 0xffff));
	} else if (((x2 - x1) > 0xff) || ((y2 - y1) > 0xff)) {
		OUT_RING(ring, 0xf0000000 | (x1 << 16) | y1);
		OUT_RING(ring, 0x7c0001f1);
		OUT_RING(ring, (((x2 - x1) & 0xffff) << 16) | ((y2 - y1) & 0xffff));
	} else {
		OUT_RING(ring, 0xf0000000 | (x1 << 16) | y1);
		OUT_RING(ring, 0xf1000000 | ((x2 - x1) << 16) | (y2 - y1));
	}
#else
	OUT_RING(ring, 0x7c0002f0);
	OUT_RING(ring, ((x1 & 0xffff) << 16) | (y1 & 0xffff));
	OUT_RING(ring, (((x2 - x1) & 0xffff) << 16) | ((y2 - y1) & 0xffff));
#endif
	OUT_RING  (ring, 0x7c0001ff);
	OUT_RING  (ring, exa->fill);
	END_RING  (ring);

	/* Notes:
	 *  [1] not sure why it is h*2.. maybe it is shifted extra bit over?
	 *  [2] these appear to differ even between two identical blits.. maybe
	 *      they are random garbage?
	 */
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
	MSM_LOCALS(pDstPixmap);

	TRACE_EXA("%p {%dx%d,%d} <- %p {%dx%d,%d}",
			pDstPixmap, pDstPixmap->drawable.width,
			pDstPixmap->drawable.height, pDstPixmap->devKind,
			pSrcPixmap, pSrcPixmap->drawable.width,
			pSrcPixmap->drawable.height, pSrcPixmap->devKind);

	EXA_FAIL_IF(planemask != FB_ALLONES);
	EXA_FAIL_IF(alu != GXcopy);

	// TODO other color formats
	EXA_FAIL_IF(pSrcPixmap->drawable.bitsPerPixel != 32);
	EXA_FAIL_IF(pDstPixmap->drawable.bitsPerPixel != 32);

	exa->src = pSrcPixmap;

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
	MSM_LOCALS(pDstPixmap);
	PixmapPtr pSrcPixmap = exa->src;
	struct msm_drm_bo *dst_bo = msm_get_pixmap_bo(pDstPixmap);
	struct msm_drm_bo *src_bo = msm_get_pixmap_bo(pSrcPixmap);
	uint32_t dw, dh, dp, sw, sh, sp;

	dw = pDstPixmap->drawable.width;
	dh = pDstPixmap->drawable.height;
	sw = pSrcPixmap->drawable.width;
	sh = pSrcPixmap->drawable.height;

	/* pitch specified in units of 32 bytes, it appears.. not quite sure
	 * max size yet, but I think 11 or 12 bits..
	 */
	dp = (msm_pixmap_get_pitch(pDstPixmap) / 32) & 0xfff;
	sp = (msm_pixmap_get_pitch(pSrcPixmap) / 32) & 0xfff;

	TRACE_EXA("srcX=%d\tsrcY=%d\tdstX=%d\tdstY=%d\twidth=%d\theight=%d",
			srcX, srcY, dstX, dstY, width, height);

	BEGIN_RING(ring, 45);
	OUT_RING  (ring, 0x0c000000);
	OUT_RING  (ring, 0x11000000);
	OUT_RING  (ring, 0xd0030000);
	/* setup for dst parameters appears similar to solid: */
	OUT_RING  (ring, 0xd2000000 | (((dh * 2) & 0xfff) << 12) | (dw & 0xfff));
	OUT_RING  (ring, 0x01007000 | dp);
	OUT_RING  (ring, 0x7c000100);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d3);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d1);
	OUT_RING  (ring, 0x40007000 | dp);
	OUT_RING  (ring, 0xd5000000);
	/* from here, dst params differ from solid: */
	OUT_RING  (ring, 0x0c000000);
	OUT_RING  (ring, 0x08000000 | ((dw - 1) & 0xfff) << 12);
	OUT_RING  (ring, 0x09000000 | ((dh - 1) & 0xfff) << 12);
	OUT_RING  (ring, 0x7c00020a);
	OUT_RING  (ring, 0xff000000);
	OUT_RING  (ring, 0xff000000);
	OUT_RING  (ring, 0x11000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x7c0003d1);
	/* setup of src parameters: */
	OUT_RING  (ring, 0x40007000 | sp);
	/* possibly width/height are 13 bits.. this is similar to  dst params
	 * in copy and solid width the 'd2' in high byte.. low bit of d2 is
	 * '0' which would support the 13 bit sizes theory:
	 * TODO add some tests to confirm w/h size theory
	 */
	OUT_RING  (ring, (((sh * 2) & 0xfff) << 12) | (sw & 0xfff));
	OUT_RELOC (ring, src_bo);
	OUT_RING  (ring, 0xd5000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0e000002);
	/* again, like solid, there seems to be multiple ways to encode the coords,
	 * depending on the size but I think we can go wit worst-case:
	 */
	OUT_RING  (ring, 0x7c0003f0);
	OUT_RING  (ring, (dstX & 0xffff) << 16 | (dstY & 0xffff));
	OUT_RING  (ring, (width & 0xfff) << 16 | (height & 0xffff));
	OUT_RING  (ring, (srcX & 0xffff) << 16 | (srcY & 0xffff));
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	END_RING  (ring);
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
	MSM_LOCALS(pDstPicture->pDrawable);
	int idx = 0;

	TRACE_EXA("op:%02d: %p {%08x, %d} <- %p {%08x, %d} (%p {%08x, %d})", op,
			pDstPicture, pDstPicture->format, pDstPicture->repeat,
			pSrcPicture, pSrcPicture->format, pSrcPicture->repeat,
			pMaskPicture, pMaskPicture ? pMaskPicture->format : 0,
			pMaskPicture ? pMaskPicture->repeat : 0);

	// TODO proper handling for RGB vs BGR!

	EXA_FAIL_IF((pDstPicture->format != PICT_a8r8g8b8) &&
			(pDstPicture->format != PICT_a8b8g8r8) &&
			(pDstPicture->format != PICT_x8r8g8b8) &&
			(pDstPicture->format != PICT_x8b8g8r8) &&
			(pDstPicture->format != PICT_a8));
	EXA_FAIL_IF((pSrcPicture->format != PICT_a8r8g8b8) &&
			(pSrcPicture->format != PICT_a8b8g8r8) &&
			(pSrcPicture->format != PICT_x8r8g8b8) &&
			(pSrcPicture->format != PICT_x8b8g8r8) &&
			(pSrcPicture->format != PICT_a8));

	if (pMaskPicture) {
		EXA_FAIL_IF((pMaskPicture->format != PICT_a8r8g8b8) &&
				(pMaskPicture->format != PICT_a8b8g8r8) &&
				(pMaskPicture->format != PICT_x8r8g8b8) &&
				(pMaskPicture->format != PICT_x8b8g8r8) &&
				(pMaskPicture->format != PICT_a8));
		EXA_FAIL_IF(pMaskPicture->transform);
		EXA_FAIL_IF(pMaskPicture->repeat);
		/* this doesn't appear to be supported by libC2D2.. although
		 * perhaps it is supported by the hw?  It might be worth
		 * experimenting with this at some point
		 */
		EXA_FAIL_IF(pMaskPicture->componentAlpha);
	}

	// TODO src add transforms later:
	EXA_FAIL_IF(pSrcPicture->transform);
	EXA_FAIL_IF(pSrcPicture->repeat);

	if (PICT_FORMAT_A(pSrcPicture->format))
		idx += 2;
	if (PICT_FORMAT_A(pDstPicture->format))
		idx += 1;

	/* check for unsupported op: */
	EXA_FAIL_IF((op >= ARRAY_SIZE(composite_op_dwords[idx])) ||
			!composite_op_dwords[idx][op][1]);

	// TODO anything we need to reject early?

	// TODO figure out a way to deal w/ maskX/maskY.. for now reject mask:
	EXA_FAIL_IF(pMaskPicture);

	exa->op_dwords = composite_op_dwords[idx][op];
	exa->dstpic    = pDstPicture;
	exa->srcpic    = pSrcPicture;
	exa->maskpic   = pMaskPicture;

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
 *   Note: componentAlpha means to treat each R/G/B channel as an independent
 *   alpha value for the corresponding channel in the src.
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
	MSM_LOCALS(pDst);

	TRACE_EXA("%p {%dx%d,%d} <- %p {%dx%d,%d} (%p {%dx%d,%d})",
			pDst, pDst->drawable.width, pDst->drawable.height, pDst->devKind,
			pSrc, pSrc->drawable.width, pSrc->drawable.height, pSrc->devKind,
			pMask, pMask ? pMask->drawable.width : 0, pMask ? pMask->drawable.height : 0,
			pMask ? pMask->devKind : 0);

	// TODO revisit repeat..
	EXA_FAIL_IF(pSrcPicture->repeat &&
			((pSrc->drawable.width != 1) || (pSrc->drawable.height != 1)));

	if (pMaskPicture) {
		EXA_FAIL_IF(pMaskPicture->repeat &&
				((pMask->drawable.width != 1) || (pMask->drawable.height != 1)));
	}

	exa->src  = pSrc;
	exa->mask = pMask;

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
MSMComposite(PixmapPtr pDstPixmap, int srcX, int srcY, int maskX, int maskY,
		int dstX, int dstY, int width, int height)
{
	MSM_LOCALS(pDstPixmap);
	PixmapPtr pSrcPixmap = exa->src;
	PixmapPtr pMaskPixmap = exa->mask;
	struct msm_drm_bo *dst_bo = msm_get_pixmap_bo(pDstPixmap);
	struct msm_drm_bo *src_bo = msm_get_pixmap_bo(pSrcPixmap);
	struct msm_drm_bo *mask_bo = NULL;
	uint32_t dw, dh, dp, sw, sh, sp, mw, mh, mp;

	dw = pDstPixmap->drawable.width;
	dh = pDstPixmap->drawable.height;
	sw = pSrcPixmap->drawable.width;
	sh = pSrcPixmap->drawable.height;

	/* pitch specified in units of 32 bytes, it appears.. not quite sure
	 * max size yet, but I think 11 or 12 bits..
	 */
	dp = (msm_pixmap_get_pitch(pDstPixmap) / 32) & 0xfff;
	sp = (msm_pixmap_get_pitch(pSrcPixmap) / 32) & 0xfff;

	if (pMaskPixmap) {
		mask_bo = msm_get_pixmap_bo(pMaskPixmap);
		mw = pMaskPixmap->drawable.width;
		mh = pMaskPixmap->drawable.height;
		mp = (msm_pixmap_get_pitch(pMaskPixmap) / 32) & 0xfff;
	}

	TRACE_EXA("srcX=%d\tsrcY=%d\tmaskX=%d\tmaskY=%d\tdstX=%d\tdstY=%d\twidth=%d\theight=%d",
			srcX, srcY, maskX, maskY, dstX, dstY, width, height);

	BEGIN_RING(ring, 59);
	OUT_RING  (ring, 0x0c000000);
	OUT_RING  (ring, 0x11000000);
	OUT_RING  (ring, 0xd0030000);
	/* setup for dst parameters: */
	// TODO check if 13 bit
	OUT_RING  (ring, 0xd2000000 | (((dh * 2) & 0xfff) << 12) | (dw & 0xfff));
	OUT_RING  (ring, 0x01000000 | dp | ((pDstPixmap->drawable.depth == 8) ? 0xe000 : 0x7000));
	OUT_RING  (ring, 0x7c000100);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d3);
	OUT_RELOC (ring, dst_bo);
	OUT_RING  (ring, 0x7c0001d1);
	OUT_RING  (ring, 0x40000000 | dp | ((pDstPixmap->drawable.depth == 8) ? 0xe000 : 0x7000));
	OUT_RING  (ring, 0xd5000000);
	/* from here, dst params differ from solid: */
	OUT_RING  (ring, 0x0c000000);
	OUT_RING  (ring, 0x08000000 | ((dw - 1) & 0xfff) << 12);
	OUT_RING  (ring, 0x09000000 | ((dh - 1) & 0xfff) << 12);

	if (!PICT_FORMAT_A(exa->dstpic->format)) {
		OUT_RING(ring, 0x7c00020a);
		OUT_RING(ring, 0xff000000);
		OUT_RING(ring, 0xff000000);
		OUT_RING(ring, 0x7c0001b2);
		OUT_RING(ring, 0xff000000);
	} else {
		OUT_RING(ring, 0x0a000000);
		OUT_RING(ring, 0x0b000000);
	}

	if (!PICT_FORMAT_A(exa->srcpic->format)) {
		OUT_RING(ring, 0x7c0001b0);
		OUT_RING(ring, 0xff000000);
	}

	if (exa->op_dwords[0])
		OUT_RING(ring, exa->op_dwords[0]);
	OUT_RING(ring, exa->op_dwords[1]);
	if (exa->op_dwords[2])
		OUT_RING(ring, exa->op_dwords[2]);
	OUT_RING(ring, exa->op_dwords[3]);

	OUT_RING  (ring, 0x11000060 | (pMaskPixmap ? 0 : 0x80) | (PICT_FORMAT_A(exa->dstpic->format) ? 0 : 0x00200000));
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x7c0003d1);
	/* setup of src parameters: */
	OUT_RING  (ring, 0x40000000 | sp | ((pSrcPixmap->drawable.depth == 8) ? 0xe000 : 0x7000));
	// TODO check if 13 bit
	OUT_RING  (ring, (((sh * 2) & 0xfff) << 12) | (sw & 0xfff));
	OUT_RELOC (ring, src_bo);
	OUT_RING  (ring, 0xd5000000);
	if (pMaskPixmap) {
		/* XXX C2D2 doesn't give a way to specify maskX/maskY, so not
		 * entirely sure if this is a hw limitation, or if not how the
		 * mask coords are specified in the cmdstream.  One possible
		 * approach is ptr arithmetic on the gpuaddr
		 */
		OUT_RING (ring, 0xd0020000);
		OUT_RING (ring, 0x7c0003d1);
		OUT_RING (ring, 0x40000000 | mp | ((pMaskPixmap->drawable.depth == 8) ? 0xe000 : 0x7000));
		// TODO check if 13 bit
		OUT_RING  (ring, (((mh * 2) & 0xfff) << 12) | (mw & 0xfff));
		OUT_RELOC(ring, mask_bo);
		OUT_RING (ring, 0xd5000080);
	}
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0f00000a);
	OUT_RING  (ring, 0x0e000003 | (pMaskPixmap ? 0x04 : 0));
	OUT_RING  (ring, 0x7c0003f0);
	OUT_RING  (ring, (dstX & 0xffff) << 16 | (dstY & 0xffff));
	OUT_RING  (ring, (width & 0xfff) << 16 | (height & 0xffff));
	OUT_RING  (ring, (srcX & 0xffff) << 16 | (srcY & 0xffff));
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	OUT_RING  (ring, 0xd0000000);
	END_RING  (ring);

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

	if (pMsm->pExa == NULL) {
		pMsm->pExa = exaDriverAlloc();
		pMsm->exa = calloc(1, sizeof(*pMsm->exa));
	}

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
