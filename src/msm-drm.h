/* msm-drm.h
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

#ifndef MSM_DRM_H_
#define MSM_DRM_H_

#include <drm.h>
#include <drm/kgsl_drm.h>

#define MSM_DRM_MEMTYPE_EBI          0
#define MSM_DRM_MEMTYPE_SMI          1
#define MSM_DRM_MEMTYPE_KMEM         2
#define MSM_DRM_MEMTYPE_KMEM_NOCACHE 3

struct msm_drm_bo {
	int fd;
	unsigned int name;
	int memtype;
	unsigned int size;
	unsigned int handle;
	int count;
	void *hostptr;
	void *virtaddr[3];
	unsigned int offsets[3];
	unsigned int gpuaddr[3];
	int ref;
	int active;
	unsigned long long offset;
};

int msm_drm_init(int fd);
struct msm_drm_bo *msm_drm_bo_create(MSMPtr pMsm, int fd, int size, int type);
int msm_drm_bo_flink(struct msm_drm_bo *bo, unsigned int *name);
void msm_drm_bo_free(MSMPtr pMsm, struct msm_drm_bo *bo);
void msm_drm_bo_unmap(struct msm_drm_bo *bo);
int msm_drm_bo_map(struct msm_drm_bo *bo);
int msm_drm_bo_bind_gpu(struct msm_drm_bo *bo);
int msm_drm_bo_alloc(struct msm_drm_bo *bo);
int msm_drm_bo_set_memtype(struct msm_drm_bo *bo, int type);
int msm_drm_bo_get_memtype(struct msm_drm_bo *bo);


int msm_drm_bo_set_bufcount(struct msm_drm_bo *bo, int count);
int msm_drm_bo_swap_buffers(struct msm_drm_bo *bo);

int msm_drm_bo_support_swap(int fd);
struct msm_drm_bo *msm_drm_bo_create_fb(MSMPtr pMsm, int drmfd, int fbfd,
		int size);
#endif
