/* msm-output.c
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
#include "xf86i2c.h"
#include "xf86Crtc.h"
#include "xf86_OSlib.h"
#include "X11/Xatom.h"
#include "randrstr.h"

#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif
#include "msm.h"

static void
MSMOutputCreateResources(xf86OutputPtr output)
{
	/* No custom properties are supported */
}

static Bool
MSMOutputSetProperty(xf86OutputPtr output, Atom property,
		RRPropertyValuePtr value)
{
	/* No custom properties are supported */
	return TRUE;
}

static void
MSMOutputDPMS(xf86OutputPtr output, int mode)
{
	/* DPMS is handled at the CRTC */
}

static void
MSMOutputPrepare(xf86OutputPtr output)
{
}

static void
MSMOutputCommit(xf86OutputPtr output)
{
}

static void
MSMOutputSave(xf86OutputPtr output)
{
}

static void
MSMOutputRestore(xf86OutputPtr output)
{
}

static int
MSMOutputModeValid(xf86OutputPtr output, DisplayModePtr pMode)
{
	return MODE_OK;
}

static Bool
MSMOutputModeFixup(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjmode)
{
	return TRUE;
}

static void
MSMOutputModeSet(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjmode)
{
	/* Nothing to do on the output side */
}

static xf86OutputStatus
MSMOutputDetect(xf86OutputPtr output)
{
	return XF86OutputStatusConnected;
}

static DisplayModePtr
MSMOutputGetModes(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;

	MSMPtr pMsm = MSMPTR(pScrn);

	DisplayModePtr modes = NULL;

	modes = xf86DuplicateMode(&pMsm->default_mode);
	return modes;
}

static void
MSMOutputDestroy(xf86OutputPtr output)
{
}

static const xf86OutputFuncsRec MSMOutputFuncs = {
		.create_resources = MSMOutputCreateResources,
		.dpms = MSMOutputDPMS,
		.save = MSMOutputSave,
		.restore = MSMOutputRestore,
		.mode_valid = MSMOutputModeValid,
		.mode_fixup = MSMOutputModeFixup,
		.prepare = MSMOutputPrepare,
		.mode_set = MSMOutputModeSet,
		.commit = MSMOutputCommit,
		.detect = MSMOutputDetect,
		.get_modes = MSMOutputGetModes,
		.set_property = MSMOutputSetProperty,
		.destroy = MSMOutputDestroy
};

void
MSMOutputSetup(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86OutputPtr output;

	/*For primary display*/
	output = xf86OutputCreate(pScrn, &MSMOutputFuncs, "default");

	output->interlaceAllowed = FALSE;
	output->doubleScanAllowed = FALSE;

	/* FIXME: Set monitor size here? */
	output->possible_crtcs = 1;
	output->driver_private = NULL;
	output->crtc = xf86_config->crtc[0];
}
