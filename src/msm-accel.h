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
#include "freedreno_ringbuffer.h"

#define LOG_DWORDS 0

#define STATE_SIZE  0x140

static inline void
OUT_RING(struct fd_ringbuffer *ring, unsigned data)
{
	if (LOG_DWORDS) {
		ErrorF("ring[%p]: OUT_RING   %04x:  %08x\n", ring->pipe,
				(uint32_t)(ring->cur - ring->last_start), data);
	}
	fd_ringbuffer_emit(ring, data);
}

static inline void
OUT_RELOC(struct fd_ringbuffer *ring, struct fd_bo *bo)
{
	/* we don't really do reloc's, so just emits the gpuaddr for a bo..
	 * (but someday we might do something more clever..)
	 */
	if (LOG_DWORDS) {
		ErrorF("ring[%p]: OUT_RELOC  %04x:  %p\n", ring->pipe,
				(uint32_t)(ring->cur - ring->last_start), bo);
	}
	fd_ringbuffer_emit_reloc(ring, bo, 0);
}

static inline void
BEGIN_RING(struct fd_ringbuffer *ring, int size)
{
#if 0
	/* current kernel side just expects one cmd packet per ISSUEIBCMDS: */
	size += 11;       /* common header/footer */

	if ((ring->cur + size) > ring->end)
		fd_ringbuffer_flush(ring, size);
#endif

	/* each packet seems to carry the address/size of next (w/ 0x00000000
	 * meaning no branch, next packet follows).  Each cmd packet is preceded
	 * by a dummy packet to give the size of the next..
	 */
	OUT_RING (ring, 0x7c000275);
	OUT_RING (ring, 0x00000000);	/* next address */
	OUT_RING (ring, 0x00000000);	/* next size */
	OUT_RING (ring, 0x7c000134);
	OUT_RING (ring, 0x00000000);

	OUT_RING (ring, 0x7c000275);
	OUT_RING (ring, 0x00000000);	/* fixed up by kernel */
	OUT_RING (ring, 0x00000000);	/* fixed up by kernel */
}

static inline void
END_RING(struct fd_ringbuffer *ring)
{
	/* This appears to be common end of packet: */
	OUT_RING(ring, 0xfe000003);
	OUT_RING(ring, 0x7f000000);
	OUT_RING(ring, 0x7f000000);

	/* We only support on cmd at a time until issueibcmds ioctl is fixed
	 * to work sanely..
	 */
	fd_ringbuffer_flush(ring);
	fd_ringbuffer_reset(ring);
}


#endif /* MSM_ACCEL_H_ */
