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


struct fd_bo *
msm_get_pixmap_bo(PixmapPtr pix)
{
	struct msm_pixmap_priv *priv = exaGetPixmapDriverPrivate(pix);

	if (priv && priv->bo)
		return priv->bo;

	assert(!priv);

	return NULL;
}

void
msm_set_pixmap_bo(PixmapPtr pix, struct fd_bo *bo)
{
	struct msm_pixmap_priv *priv = exaGetPixmapDriverPrivate(pix);

	if (priv) {
		struct fd_bo *old_bo = priv->bo;
		priv->bo = bo ? fd_bo_ref(bo) : NULL;
		if (old_bo)
			fd_bo_del(old_bo);
	}
}

int
msm_get_pixmap_name(PixmapPtr pix, unsigned int *name, unsigned int *pitch)
{
	int ret = -1;
	struct fd_bo *bo = msm_get_pixmap_bo(pix);
	if (bo) {
		*pitch = exaGetPixmapPitch(pix);
		ret = fd_bo_get_name(bo, name);
	}
	return ret;
}

void
msm_pixmap_exchange(PixmapPtr a, PixmapPtr b)
{
	struct msm_pixmap_priv *apriv = exaGetPixmapDriverPrivate(a);
	struct msm_pixmap_priv *bpriv = exaGetPixmapDriverPrivate(b);
	exchange(apriv->bo, bpriv->bo);
}
