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

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/msm_kgsl.h>

#include "msm.h"
#include "msm-drm.h"
#include "msm-accel.h"

/* matching buffer info:
                len:            00001000
                gpuaddr:        66142000

                len:            00009000
                gpuaddr:        66276000

                len:            00081000
                gpuaddr:        66280000
 */
static uint32_t initial_state[] = {
		 0x7c000275, 0x00000000, 0x00050005, 0x7c000129,
		 0x00000000, 0x7c00012a, 0x00000000, 0x7c00012b,
		 0x00000000, 0x7c00010f, 0x00000000, 0x7c000108,
		 0x00000000, 0x7c000109, 0x00000000, 0x7c000100,
		 0x00000000, 0x7c000101, 0x00000000, 0x7c000110,
		 0x00000000, 0x7c0001d0, 0x00000000, 0x7c0001d4,
		 0x00000000, 0x7c00010c, 0x00000000, 0x7c00010e,
		 0x00000000, 0x7c00010d, 0x00000000, 0x7c00010b,
		 0x00000000, 0x7c00010a, 0x00000000, 0x7c000111,
		 0x00000000, 0x7c000114, 0x00000000, 0x7c000115,
		 0x00000000, 0x7c000116, 0x00000000, 0x7c000117,
		 0x00000000, 0x7c000118, 0x00000000, 0x7c000119,
		 0x00000000, 0x7c00011a, 0x00000000, 0x7c00011b,
		 0x00000000, 0x7c00011c, 0x00000000, 0x7c00011d,
		 0x00000000, 0x7c00011e, 0x00000000, 0x7c00011f,
		 0x00000000, 0x7c000124, 0x00000000, 0x7c000125,
		 0x00000000, 0x7c000127, 0x00000000, 0x7c000128,
		 0x00000000, 0x7b00015e, 0x00000000, 0x7b000161,
		 0x00000000, 0x7b000165, 0x00000000, 0x7b000166,
		 0x00000000, 0x7b00016e, 0x00000000, 0x7c00016f,
		 0x00000000, 0x7b000165, 0x00000000, 0x7b000154,
		 0x00000000, 0x7b000155, 0x00000000, 0x7b000153,
		 0x00000000, 0x7b000168, 0x00000000, 0x7b000160,
		 0x00000000, 0x7b000150, 0x00000000, 0x7b000156,
		 0x00000000, 0x7b000157, 0x00000000, 0x7b000158,
		 0x00000000, 0x7b000159, 0x00000000, 0x7b000152,
		 0x00000000, 0x7b000151, 0x00000000, 0x7b000156,
		 0x00000000, 0x7c00017f, 0x00000000, 0x7c00017f,
		 0x00000000, 0x7c00017f, 0x00000000, 0x7c00017f,
		 0x00000000, 0x7f000000, 0x7f000000, 0x7c000129,
/**/	 0x66142000, 0x7c00012a, 0x66276000, 0x7c00012b,
/**/	 0x66280000, 0x7c0001e2, 0x00000000, 0x7c0001e3,
		 0x00000000, 0x7c0001e4, 0x00000000, 0x7c0001e5,
		 0x00000000, 0x7c0001e6, 0x00000000, 0x7c0001e7,
		 0x00000000, 0x7c0001c0, 0x00000000, 0x7c0001c1,
		 0x00000000, 0x7c0001c2, 0x00000000, 0x7c0001c3,
		 0x00000000, 0x7c0001c4, 0x00000000, 0x7c0001c5,
		 0x00000000, 0x7c0001c6, 0x00000000, 0x7c0001c7,
		 0x00000000, 0x7c0001c8, 0x00000000, 0x7c0001c9,
		 0x00000000, 0x7c0001ca, 0x00000000, 0x7c0001d1,
		 0x00000000, 0x7c0001d2, 0x00000000, 0x7c0001d4,
		 0x00000000, 0x7c0001d3, 0x00000000, 0x7c0001d5,
		 0x00000000, 0x7c0001d0, 0x00000000, 0x7c0001e0,
		 0x00000000, 0x7c0001e1, 0x00000000, 0x7c0001e2,
		 0x00000000, 0x7c0001e3, 0x00000000, 0x7c0001e4,
		 0x00000000, 0x7c0001e5, 0x00000000, 0x7c0001e6,
		 0x00000000, 0x7c0001e7, 0x00000000, 0x7c0001c0,
		 0x00000000, 0x7c0001c1, 0x00000000, 0x7c0001c2,
		 0x00000000, 0x7c0001c3, 0x00000000, 0x7c0001c4,
		 0x00000000, 0x7c0001c5, 0x00000000, 0x7c0001c6,
		 0x00000000, 0x7c0001c7, 0x00000000, 0x7c0001c8,
		 0x00000000, 0x7c0001c9, 0x00000000, 0x7c0001ca,
		 0x00000000, 0x7c0001d1, 0x00000000, 0x7c0001d2,
		 0x00000000, 0x7c0001d4, 0x00000000, 0x7c0001d3,
		 0x00000000, 0x7c0001d5, 0x00000000, 0x7c0001d0,
		 0x00000000, 0x7c0001e0, 0x00000000, 0x7c0001e1,
		 0x00000000, 0x7c0001e2, 0x00000000, 0x7c0001e3,
		 0x00000000, 0x7c0001e4, 0x00000000, 0x7c0001e5,
		 0x00000000, 0x7c0001e6, 0x00000000, 0x7c0001e7,
		 0x00000000, 0x7c0001c0, 0x00000000, 0x7c0001c1,
		 0x00000000, 0x7c0001c2, 0x00000000, 0x7c0001c3,
		 0x00000000, 0x7c0001c4, 0x00000000, 0x7c0001c5,
		 0x00000000, 0x7c0001c6, 0x00000000, 0x7c0001c7,
		 0x00000000, 0x7c0001c8, 0x00000000, 0x7c0001c9,
		 0x00000000, 0x7c0001ca, 0x00000000, 0x7c0001d1,
		 0x00000000, 0x7c0001d2, 0x00000000, 0x7c0001d4,
		 0x00000000, 0x7c0001d3, 0x00000000, 0x7c0001d5,
		 0x00000000, 0x7c0001d0, 0x00000000, 0x7c0001e0,
		 0x00000000, 0x7c0001e1, 0x00000000, 0x7c0001e2,
		 0x00000000, 0x7c0001e3, 0x00000000, 0x7c0001e4,
		 0x00000000, 0x7c0001e5, 0x00000000, 0x7c0001e6,
		 0x00000000, 0x7c0001e7, 0x00000000, 0x7c0001c0,
		 0x00000000, 0x7c0001c1, 0x00000000, 0x7c0001c2,
		 0x00000000, 0x7c0001c3, 0x00000000, 0x7c0001c4,
		 0x00000000, 0x7c0001c5, 0x00000000, 0x7c0001c6,
		 0x00000000, 0x7c0001c7, 0x00000000, 0x7c0001c8,
		 0x00000000, 0x7c0001c9, 0x00000000, 0x7c0001ca,
		 0x00000000, 0x7c0001d1, 0x00000000, 0x7c0001d2,
		 0x00000000, 0x7c0001d4, 0x00000000, 0x7c0001d3,
		 0x00000000, 0x7c0001d5, 0x00000000, 0x7f000000,
};

/* because kgsl tries to validate the gpuaddr on kernel side in ISSUEIBCMDS,
 * we can't use normal gem bo's for ringbuffer..  someday the kernel part
 * needs to be reworked into a single sane drm driver :-/
 */
struct kgsl_vmalloc_bo {
	void    *hostptr;
	uint32_t gpuaddr;
	uint32_t size;
};

static struct kgsl_vmalloc_bo *
gpumem_create(MSMPtr pMsm, int size)
{
	struct kgsl_vmalloc_bo *bo;

	bo = calloc(1, sizeof *bo);
	if (!bo) {
		ErrorF("failed to allocate kgsl bo\n");
		return NULL;
	}

	bo->size = size;
	bo->hostptr = mmap(NULL, size, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);

	if (bo->hostptr) {
		struct kgsl_sharedmem_from_vmalloc req = {
				.hostptr = (unsigned long)bo->hostptr,
				.flags   = 1,
		};
		int ret = ioctl(pMsm->kgsl_3d0_fd,
				IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC, &req);
		if (ret) {
			ErrorF("IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC failed: %d (%s)\n",
					ret, strerror(errno));
			goto fail;
		}
		bo->gpuaddr = req.gpuaddr;
		return bo;
	}

fail:
	if (bo->hostptr)
		munmap(bo->hostptr, size);
	free(bo);
	return NULL;
}

static struct kgsl_ringbuffer *
kgsl_ringbuffer_init(ScrnInfoPtr pScrn, int fd)
{
	MSMPtr pMsm = MSMPTR(pScrn);
	struct kgsl_drawctxt_create req = {
			.flags = 0,
	};
	struct kgsl_ringbuffer *ring;
	int ret;

	ret = ioctl(fd, IOCTL_KGSL_DRAWCTXT_CREATE, &req);
	if (ret) {
		ERROR_MSG("failed to allocate context: %d (%s)",
				ret, strerror(errno));
		return NULL;
	}

	/* allocate ringbuffer: */
	ring = calloc(1, sizeof *ring);
	if (!ring) {
		ERROR_MSG("allocation failed");
		return NULL;
	}

	ring->fd = fd;
	ring->drawctxt_id = req.drawctxt_id;

	ring->context_bos[0] = gpumem_create(pMsm,  0x1000);
	ring->context_bos[1] = gpumem_create(pMsm,  0x9000);
	ring->context_bos[2] = gpumem_create(pMsm, 0x81000);

	ring->cmdstream = gpumem_create(pMsm, 0x5000);

	ring->start = ring->cmdstream->hostptr;
	ring->end   = ring->start + ring->cmdstream->size;

	/* for now, until state packet is understood, just use a pre-canned
	 * state captured from libC2D2 test, and fix up the gpu addresses
	 */
	memcpy(ring->start, initial_state, STATE_SIZE * sizeof(uint32_t));
	ring->start[120] = ring->context_bos[0]->gpuaddr;
	ring->start[122] = ring->context_bos[1]->gpuaddr;
	ring->start[124] = ring->context_bos[2]->gpuaddr;

	INFO_MSG("context buffers: %08x, %08x, %08x",
			ring->start[120], ring->start[122], ring->start[124]);
	INFO_MSG("cmdstream buffer: %08x", ring->cmdstream->gpuaddr);

	/* do initial setup: */
	kgsl_ringbuffer_flush(ring, 0);

	return ring;
}

static void
kgsl_ringbuffer_start(struct kgsl_ringbuffer *ring)
{
	BEGIN_RING(ring, 8, "start");
	OUT_RING  (ring, 0x7c000329);
	OUT_RING  (ring, ring->context_bos[0]->gpuaddr);
	OUT_RING  (ring, ring->context_bos[1]->gpuaddr);
	OUT_RING  (ring, ring->context_bos[2]->gpuaddr);
	OUT_RING  (ring, 0x11000000);
	OUT_RING  (ring, 0x10fff000);
	OUT_RING  (ring, 0x10ffffff);
	OUT_RING  (ring, 0x0d000404);
	END_RING  (ring);
}

int
kgsl_ringbuffer_flush(struct kgsl_ringbuffer *ring, int min)
{
	/* if nothing emitted yet, then bail */
	if (ring->cur == &ring->start[STATE_SIZE + 8])
		return 0;

	if (ring->last_start != ring->cur) {
		uint32_t last_size;
		struct kgsl_ibdesc ibdesc = {
				.gpuaddr     = ring->cmdstream->gpuaddr,
				.hostptr     = ring->cmdstream->hostptr,
				.sizedwords  = 0x145,
		};
		struct kgsl_ringbuffer_issueibcmds req = {
				.drawctxt_id = ring->drawctxt_id,
				.ibdesc_addr = (unsigned long)&ibdesc,
				.numibs      = 1,
				.flags       = KGSL_CONTEXT_SUBMIT_IB_LIST,
				/* z180_cmdstream_issueibcmds() is made of fail:
				 */
				.timestamp   = (unsigned long)ring->cmdstream->hostptr,
		};
		int ret;

		OUT_RING(ring, 0xfe000003);
		OUT_RING(ring, 0x7f000000);
		OUT_RING(ring, 0x7f000000);


		/* fix up size field in last cmd packet */
		last_size = (uint32_t)(ring->cur - ring->last_start) + 8;
		ring->last_start[-6] = last_size;

		ret = ioctl(ring->fd, IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS, &req);
		if (ret)
			ErrorF("issueibcmds failed!  %d (%s)\n", ret, strerror(errno));

		ring->timestamp = req.timestamp;

		// TODO until the ISSUEIBCMDS is re-worked on kernel side,
		// we can only do one blit at a time:
		kgsl_ringbuffer_wait(ring);
	}

	ring->cur = &ring->start[STATE_SIZE];
	ring->last_start = ring->cur;

	OUT_RING (ring, 0x7c000275);
	OUT_RING (ring, 0x00000000);	/* next address */
	OUT_RING (ring, 0x00000000);	/* next size */
	OUT_RING (ring, 0x7c000134);
	OUT_RING (ring, 0x00000000);

	OUT_RING (ring, 0x7c000275);
	OUT_RING (ring, 0x00000000);	/* fixed up by kernel */
	OUT_RING (ring, 0x00000000);	/* fixed up by kernel */

	return 0;
}

uint32_t kgsl_ringbuffer_timestamp(struct kgsl_ringbuffer *ring)
{
	struct kgsl_cmdstream_readtimestamp req = {
			.type = KGSL_TIMESTAMP_RETIRED,
	};
	int ret;
	ret = ioctl(ring->fd, IOCTL_KGSL_CMDSTREAM_READTIMESTAMP, &req);
	if (ret)
		ErrorF("readtimestamp failed! %d (%s)\n", ret, strerror(errno));
	return req.timestamp;
}

void kgsl_ringbuffer_wait(struct kgsl_ringbuffer *ring)
{
	struct kgsl_device_waittimestamp req = {
			.timestamp = ring->timestamp,
			.timeout   = ~0,
	};
	int ret;
	do {
		ret = ioctl(ring->fd, IOCTL_KGSL_DEVICE_WAITTIMESTAMP, &req);
	} while ((ret == -1) && ((errno == EINTR) || (errno == EAGAIN)));
	if (ret)
		ErrorF("waittimestamp failed! %d (%s)\n", ret, strerror(errno));
}

static Bool
getprop(int fd, enum kgsl_property_type type, void *value, int sizebytes)
{
	struct kgsl_device_getproperty req = {
			.type = type,
			.value = value,
			.sizebytes = sizebytes,
	};
	return ioctl(fd, IOCTL_KGSL_DEVICE_GETPROPERTY, &req) == 0;
}

#define GETPROP(fd, prop, x) do { \
	if (!getprop((fd), KGSL_PROP_##prop, &(x), sizeof(x))) {			\
		ERROR_MSG("failed to get property: " #prop);					\
		return FALSE;													\
	} } while (0)

Bool
MSMSetupAccel(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MSMPtr pMsm = MSMPTR(pScrn);
	int int_waits;
	struct kgsl_devinfo devinfo;
	struct kgsl_version version;
	int kgsl_3d0_fd, kgsl_2d0_fd, kgsl_2d1_fd;

	DEBUG_MSG("setup-accel");

	/* possibly we don't need all of these.. I'm just following
	 * the sequence logged from libC2D2..
	 */
	kgsl_3d0_fd = open("/dev/kgsl-3d0", O_RDWR);
	kgsl_2d0_fd = open("/dev/kgsl-2d0", O_RDWR);
	kgsl_2d1_fd = open("/dev/kgsl-2d1", O_RDWR);

	if ((kgsl_3d0_fd < 0) || (kgsl_2d0_fd < 0) || (kgsl_2d1_fd < 0)) {
		ERROR_MSG("fail!");
		return FALSE;
	}

	pMsm->kgsl_3d0_fd = kgsl_3d0_fd;

	GETPROP(kgsl_3d0_fd, INTERRUPT_WAITS, int_waits);
	GETPROP(kgsl_3d0_fd, DEVICE_INFO,     devinfo);
	GETPROP(kgsl_3d0_fd, VERSION,         version);

	INFO_MSG("Accel Info:");
	INFO_MSG(" Chip-id:         %d.%d.%d.%d",
			(devinfo.chip_id >> 24) & 0xff,
			(devinfo.chip_id >> 16) & 0xff,
			(devinfo.chip_id >>  8) & 0xff,
			(devinfo.chip_id >>  0) & 0xff);
	INFO_MSG(" Device-id:       %d", devinfo.device_id);
	INFO_MSG(" GPU-id:          %d", devinfo.gpu_id);
	INFO_MSG(" MMU enabled:     %d", devinfo.mmu_enabled);
	INFO_MSG(" Interrupt waits: %d", int_waits);
	INFO_MSG(" GMEM Base addr:  0x%08x", devinfo.gmem_gpubaseaddr);
	INFO_MSG(" GMEM size:       0x%08x", devinfo.gmem_sizebytes);
	INFO_MSG(" Driver version:  %d.%d", version.drv_major, version.drv_minor);
	INFO_MSG(" Device version:  %d.%d", version.dev_major, version.dev_minor);

	pMsm->rings[0] = kgsl_ringbuffer_init(pScrn, kgsl_2d0_fd);
	pMsm->rings[1] = kgsl_ringbuffer_init(pScrn, kgsl_2d1_fd);

	/* Make a buffer object for the framebuffer so that the GPU MMU
	 * can use it
	 */
	pMsm->fbBo = msm_drm_bo_create_fb(pMsm, pMsm->fd, pMsm->fixed_info.smem_len);

	/* for now, just using a single ringbuffer.. maybe we need to do this
	 * for both?
	 */
	kgsl_ringbuffer_start(pMsm->rings[1]);

	return MSMSetupExa(pScreen);
}
