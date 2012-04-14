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

#ifndef MSM_ACCEL_H_
#define MSM_ACCEL_H_

#include "msm.h"
#include "msm-drm.h"

#define LOG_DWORDS 1

struct kgsl_vmalloc_bo;

struct kgsl_ringbuffer {
	int fd;
	uint32_t *cur, *end, *start;
	uint32_t *last_start;

	/* the cmdstream buffer: */
	struct kgsl_vmalloc_bo *cmdstream;

	/* not sure what these are for yet: */
	struct kgsl_vmalloc_bo *context_bos[3];

	unsigned int drawctxt_id;
	unsigned int timestamp;
};

#define STATE_SIZE  0x140


int kgsl_ringbuffer_flush(struct kgsl_ringbuffer *ring, int min);
uint32_t kgsl_ringbuffer_timestamp(struct kgsl_ringbuffer *ring);
void kgsl_ringbuffer_wait(struct kgsl_ringbuffer *ring);

static inline void
OUT_RING(struct kgsl_ringbuffer *ring, uint32_t data)
{
	if (LOG_DWORDS) {
		ErrorF("ring[%d]: OUT_RING   %04x:  %08x\n", ring->fd,
				(uint32_t)(ring->cur - ring->last_start), data);
	}
	*(ring->cur++) = data;
}

static inline void
OUT_RELOC(struct kgsl_ringbuffer *ring, struct msm_drm_bo *bo)
{
	/* we don't really do reloc's, so just emits the gpuaddr for a bo..
	 * (but someday we might do something more clever..)
	 */
	if (LOG_DWORDS) {
		ErrorF("ring[%d]: OUT_RELOC  %04x:  %08x\n", ring->fd,
				(uint32_t)(ring->cur - ring->last_start),
				msm_drm_bo_gpuptr(bo));
	}
	*(ring->cur++) = msm_drm_bo_gpuptr(bo);
}

static inline void
FIRE_RING(struct kgsl_ringbuffer *ring)
{
	kgsl_ringbuffer_flush(ring, 0);
}

static inline void
BEGIN_RING(struct kgsl_ringbuffer *ring, int size, const char *name)
{
	size += 3;   /* end of packet added in kgsl_ringbuffer_flush() */

	if ((ring->cur + size) > ring->end)
		kgsl_ringbuffer_flush(ring, size);

	if (LOG_DWORDS) {
		ErrorF("ring[%d]: BEGIN_RING %04x:  %s\n", ring->fd,
				(uint32_t)(ring->cur - ring->last_start), name);
	}
}

static inline void
END_RING(struct kgsl_ringbuffer *ring)
{
	if (LOG_DWORDS) {
		ErrorF("ring[%d]: END_RING   %04x\n", ring->fd,
				(uint32_t)(ring->cur - ring->last_start));
	}

}


#endif /* MSM_ACCEL_H_ */
