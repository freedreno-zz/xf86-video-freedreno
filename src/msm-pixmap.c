/* msm-pixmap.c
 *
 * Copyright (c) 2009 - 2010 Code Aurora Forum. All rights reserved.
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

#include "msm.h"
#include "msm-drm.h"

unsigned int
msm_pixmap_gpuptr(PixmapPtr pixmap)
{
	ScreenPtr pScreen = pixmap->drawable.pScreen;
	MSMPtr pMsm = MSMPTR_FROM_PIXMAP(pixmap);

	if (msm_pixmap_in_gem(pixmap)) {
		struct msm_drm_bo *bo = msm_get_pixmap_bo(pixmap);
		if (!msm_drm_bo_bind_gpu(bo)) {
			return (unsigned int) bo->gpuaddr[bo->active];
		}
	}

	/* Return the physical address of the framebuffer */
	/* If we have a BO for the framebuffer, then bind it
       and return the adress, otherwise revert back to the
       physical address */

	if ((pScreen->GetScreenPixmap(pScreen) == pixmap) ||
			(pMsm->rotatedPixmap == pixmap)) {
		unsigned int offset = 0;
		if(pMsm->rotatedPixmap == pixmap)
			offset = pMsm->mode_info.yres * pMsm->fixed_info.line_length;
		if (pMsm->fbBo && !msm_drm_bo_bind_gpu(pMsm->fbBo))
			return (unsigned int) (pMsm->fbBo->gpuaddr[0] + offset);

		return ((unsigned int)pMsm->fixed_info.smem_start + offset);
	}

	return 0;
}

void *
msm_pixmap_hostptr(PixmapPtr pixmap)
{
	ScreenPtr pScreen = pixmap->drawable.pScreen;
	MSMPtr pMsm = MSMPTR_FROM_PIXMAP(pixmap);

	if (msm_pixmap_in_gem(pixmap)) {
		struct msm_drm_bo *bo = msm_get_pixmap_bo(pixmap);
		return (void *) bo->virtaddr[bo->active];
	}

	/* Return virtual address of the framebuffer */

	if (pScreen->GetScreenPixmap(pScreen) == pixmap)
		return pMsm->curVisiblePtr;

	return (void *) pixmap->devPrivate.ptr;
}

int
msm_pixmap_offset(PixmapPtr pixmap)
{
	return 0;
}

int
msm_pixmap_get_pitch(PixmapPtr pix)
{
	struct msm_pixmap_priv *priv = exaGetPixmapDriverPrivate(pix);

	/* We only modify the pitch for 16bpp operations */

	if (priv && priv->bo && pix->drawable.bitsPerPixel == 16) {
		return ((pix->drawable.width + 31) & ~31) *
				(pix->drawable.bitsPerPixel >> 3);
	}

	return exaGetPixmapPitch(pix);
}

Bool
msm_pixmap_in_gem(PixmapPtr pix)
{
	struct msm_pixmap_priv *priv = exaGetPixmapDriverPrivate(pix);

	if (priv && priv->bo)
		return TRUE;

	return FALSE;
}

struct msm_drm_bo *
msm_get_pixmap_bo(PixmapPtr pix)
{
	struct msm_pixmap_priv *priv = exaGetPixmapDriverPrivate(pix);

	if (priv && priv->bo) {
		/* When this fucntion is called then ensure it gets
	  allocated - if this function ever gets used outside of
	  EXA this could cause problems */

		msm_drm_bo_alloc(priv->bo);
		return priv->bo;
	}

	return NULL;
}
