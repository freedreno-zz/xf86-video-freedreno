/* msm-drm.c
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

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <xf86drm.h>

#include <drm.h>
#include <drm/kgsl_drm.h>

#include "msm.h"
#include "msm-drm.h"

static int drm_version;

static int
msm_drm_get_version(int fd)
{
	struct drm_version version;
	memset(&version, 0, sizeof(version));

	if (ioctl(fd, DRM_IOCTL_VERSION, &version)) {
		ErrorF("%s:  Unable to get the DRM version\n", __FUNCTION__);
		return -1;
	}

	drm_version = (version.version_major << 16);
	drm_version |= version.version_minor & 0xFFFF;

	return 0;
}

int
msm_drm_bo_set_memtype(struct msm_drm_bo *bo, int type)
{
	static int ebionly = 0;
	int ret;
	struct drm_kgsl_gem_memtype mtype;

	if (bo == NULL || bo->handle == 0)
		return -1;

	/* Only fail the ioctl() once - the other times just quietly
	 * force the mode to EBI - see below */

	if (ebionly) {
		bo->memtype = DRM_KGSL_GEM_TYPE_EBI;
		return 0;
	}

	switch(type) {
	case MSM_DRM_MEMTYPE_KMEM:
		bo->memtype = DRM_KGSL_GEM_TYPE_KMEM;
		break;
	case MSM_DRM_MEMTYPE_EBI:
		bo->memtype = DRM_KGSL_GEM_TYPE_EBI;
		break;
	case MSM_DRM_MEMTYPE_SMI:
		bo->memtype = DRM_KGSL_GEM_TYPE_SMI;
		break;
	case MSM_DRM_MEMTYPE_KMEM_NOCACHE:
		bo->memtype = DRM_KGSL_GEM_TYPE_KMEM_NOCACHE;
		break;
	default:
		return -1;
	}

	memset(&mtype, 0, sizeof(mtype));
	mtype.handle = bo->handle;
	mtype.type = bo->memtype;

	ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_SETMEMTYPE, &mtype);

	if (ret) {
		/* If the ioctl() isn't supported, then the legacy behavior
		 * is to put everything is in EBI */

		if (errno == EINVAL) {
			ErrorF("DRM:  DRM_IOCTL_KGSL_GEM_SETMEMTYPE is not supported.\n");
			ErrorF("      All offscreen memory will be in EBI\n");

			bo->memtype = DRM_KGSL_GEM_TYPE_EBI;

			/* Set a flag so we don't come in here and fail for every
			 * allocation */

			ebionly = 1;
			return 0;
		}
	}

	return ret;
}

int
msm_drm_bo_get_memtype(struct msm_drm_bo *bo)
{
	struct drm_kgsl_gem_memtype mtype;
	int ret;

	if (bo == NULL || bo->handle == 0)
		return -1;

	if (bo->memtype < 0) {
		memset(&mtype, 0, sizeof(mtype));
		mtype.handle = bo->handle;

		ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_SETMEMTYPE, &mtype);

		if (ret)
			return ret;
	}

	switch(bo->memtype) {
	case DRM_KGSL_GEM_TYPE_KMEM:
		return MSM_DRM_MEMTYPE_KMEM;
	case DRM_KGSL_GEM_TYPE_KMEM_NOCACHE:
		return MSM_DRM_MEMTYPE_KMEM_NOCACHE;
	case DRM_KGSL_GEM_TYPE_EBI:
		return MSM_DRM_MEMTYPE_EBI;
	case DRM_KGSL_GEM_TYPE_SMI:
		return MSM_DRM_MEMTYPE_SMI;
	}

	return -1;
}

void
_msm_drm_bo_free(struct msm_drm_bo *bo)
{
	struct drm_gem_close close;

	if (bo == NULL || bo->handle == 0)
		return;

	if (bo->hostptr)
		munmap((void *) bo->hostptr, bo->size * bo->count);

	memset(&close, 0, sizeof(close));
	close.handle = bo->handle;
	ioctl(bo->fd, DRM_IOCTL_GEM_CLOSE, &close);

	free(bo);
}

struct msm_drm_bo *
msm_drm_bo_create(MSMPtr pMsm, int fd, int size, int type)
{
	struct drm_kgsl_gem_create create;
	struct msm_drm_bo *bo;
	int ret;

	size = (size + (getpagesize() - 1)) & ~(getpagesize() - 1);

	if (size == 0)
		return NULL;

	/* We only cache the default pixmap type */
	if (pMsm->cachedBo && type == pMsm->pixmapMemtype) {
		if (pMsm->cachedBo->size == size) {
			bo = pMsm->cachedBo;
			pMsm->cachedBo = NULL;
			return bo;
		}

		_msm_drm_bo_free(pMsm->cachedBo);
		pMsm->cachedBo = NULL;
	}

	memset(&create, 0, sizeof(create));
	create.size = size;

	ret = ioctl(fd, DRM_IOCTL_KGSL_GEM_CREATE, &create);

	if (ret)
		return NULL;

	bo = calloc(1, sizeof(*bo));

	if (bo == NULL)
		return NULL;

	bo->size = size;
	bo->handle = create.handle;
	bo->fd = fd;
	bo->active = 0;
	bo->count = 1;

	/* All memory defaults to EBI */
	bo->memtype = DRM_KGSL_GEM_TYPE_EBI;

	if (msm_drm_bo_set_memtype(bo, type)) {
		ErrorF("Unable to set the memory type\n");
		_msm_drm_bo_free(bo);
		return NULL;
	}

	return bo;
}

int
msm_drm_bo_flink(struct msm_drm_bo *bo, unsigned int *name)
{
	struct drm_gem_flink flink;
	int ret;

	memset(&flink, 0, sizeof(flink));

	if (bo == NULL)
		return -1;

	flink.handle = bo->handle;
	ret = ioctl(bo->fd, DRM_IOCTL_GEM_FLINK, &flink);

	if (ret)
		return -1;

	bo->name = flink.name;

	if (name)
		*name = flink.name;

	return 0;
}

int
msm_drm_bo_alloc(struct msm_drm_bo *bo)
{
	struct drm_kgsl_gem_alloc alloc;
	int ret;

	if (bo == NULL)
		return -1;

	/* If the offset is set, then assume it has been allocated */
	if (bo->offset != 0)
		return 0;

	memset(&alloc, 0, sizeof(alloc));
	alloc.handle = bo->handle;

	ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_ALLOC, &alloc);

	if (ret) {
		/* if the ioctl isn't supported, then use the legacy PREP ioctl */

		if (errno == EINVAL) {
			struct drm_kgsl_gem_prep prep;

			ErrorF("DRM:  DRM_IOCTL_KGSL_GEM_ALLOC is not supported.\n");

			memset(&prep, 0, sizeof(prep));
			prep.handle = bo->handle;
			ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_PREP, &prep);

			if (ret)
				return -1;

			bo->offset = prep.offset;
			bo->gpuaddr[0] = prep.phys;

			return 0;
		}

		return ret;
	}

	bo->offset = alloc.offset;
	return 0;
}

int
msm_drm_bo_bind_gpu(struct msm_drm_bo *bo)
{
	struct drm_kgsl_gem_bind_gpu bind;
	int ret;

	if (bo->gpuaddr[0])
		return 0;

	if (bo == NULL)
		return -1;

	bind.handle = bo->handle;

	ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_BIND_GPU, &bind);

	if (ret)
		return ret;

	bo->gpuaddr[0] = bind.gpuptr;

	/* If the ioctls are in place, then get the buffer info to get the GPU
       addresses directly */

#ifdef DRM_IOCTL_KGSL_GEM_GET_BUFINFO
	{
		int i;
		struct drm_kgsl_gem_bufinfo bufinfo;
		bufinfo.handle = bo->handle;

		ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_GET_BUFINFO, &bufinfo);

		for(i = 0; !ret && i < bufinfo.count; i++)
			bo->gpuaddr[i] = bufinfo.gpuaddr[i];
	}
#endif

	return 0;
}

int
msm_drm_bo_map(struct msm_drm_bo *bo)
{
	int ret, i;
	unsigned int mapsize;

	if (bo == NULL)
		return -1;

	/* Already mapped */

	if (bo->hostptr != NULL)
		return 0;

	if (!bo->offset) {
		ret = msm_drm_bo_alloc(bo);

		if (ret) {
			ErrorF("DRM:  Unable to allocate: %m\n");
			return ret;
		}
	}

	mapsize = bo->size;
	bo->offsets[0] = 0;

#ifdef DRM_IOCTL_KGSL_GEM_GET_BUFINFO
	{
		struct drm_kgsl_gem_bufinfo bufinfo;
		bufinfo.handle = bo->handle;

		ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_GET_BUFINFO, &bufinfo);

		if (ret == 0) {
			for(i = 0; i < bufinfo.count; i++) {
				bo->offsets[i] = bufinfo.offset[i];
				bo->gpuaddr[i] = bufinfo.gpuaddr[i];
			}

			bo->count = bufinfo.count;
			bo->active = bufinfo.active;

			mapsize = bo->size * bo->count;
		}
	}
#endif

	bo->hostptr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED,
			bo->fd, bo->offset);

	if (bo->hostptr == MAP_FAILED) {
		ErrorF("DRM:  Unable to map: %m\n");
		return -1;
	}

	for(i = 0; i < bo->count; i++)
		bo->virtaddr[i] = (void *) bo->hostptr + bo->offsets[i];

	return 0;
}

void
msm_drm_bo_unmap(struct msm_drm_bo *bo)
{
	if (bo == NULL)
		return;

	/* For the moment, always leave buffers mapped */

#if 0
	if (bo->hostptr)
		munmap((void *) bo->hostptr, bo->size);

	bo->hostptr = 0;
#endif
}

void
msm_drm_bo_free(MSMPtr pMsm, struct msm_drm_bo *bo)
{
	if (bo == NULL || bo->handle == 0)
		return;

	if (bo->memtype == pMsm->pixmapMemtype &&
			bo->count == 1 &&
			bo->name == 0) {
		if (pMsm->cachedBo)
			_msm_drm_bo_free(pMsm->cachedBo);
		pMsm->cachedBo = bo;
	}
	else
		_msm_drm_bo_free(bo);
}

/* Set the next buffer in the list as active */

#ifdef DRM_IOCTL_KGSL_GEM_SET_ACTIVE

int msm_drm_bo_swap_buffers(struct msm_drm_bo *bo)
{
	struct drm_kgsl_gem_active active;
	int ret = 0;

	if (drm_version < 0x00020001)
		return -1;

	active.active = bo->active + 1;

	if (active.active == bo->count)
		active.active = 0;

	active.handle = bo->handle;

	ret= ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_SET_ACTIVE, &active);

	if (!ret)
		bo->active = active.active;

	return ret;
}
#else
int msm_drm_bo_swap_buffers(struct msm_drm_bo *bo)
{
	return -1;
}
#endif

#ifdef DRM_IOCTL_KGSL_GEM_SET_BUFCOUNT
int msm_drm_bo_set_bufcount(struct msm_drm_bo *bo, int count)
{
	struct drm_kgsl_gem_bufcount bcount;
	int ret;

	if (drm_version < 0x00020001)
		return -1;

	bcount.bufcount = count;
	bcount.handle = bo->handle;

	ret = ioctl(bo->fd, DRM_IOCTL_KGSL_GEM_SET_BUFCOUNT, &bcount);

	if (!ret)
		bo->count = count;

	return ret;
}

#endif

int msm_drm_bo_support_swap(int fd)
{
	if (!drm_version)
		msm_drm_get_version(fd);

	return (drm_version >= 0x00020001) ? 1 : 0;
}

/* Create a buffer object for the framebuffer */

struct msm_drm_bo *
msm_drm_bo_create_fb(MSMPtr pMsm, int drmfd, int fbfd, int size)
{
	struct drm_kgsl_gem_create_fd createfd;
	struct msm_drm_bo *bo;
	int ret;

	memset(&createfd, 0, sizeof(createfd));
	createfd.fd = fbfd;

	ret = ioctl(drmfd, DRM_IOCTL_KGSL_GEM_CREATE_FD, &createfd);

	if (ret)
		return NULL;

	bo = calloc(1, sizeof(*bo));

	if (bo == NULL)
		return NULL;

	bo->size = size;
	bo->handle = createfd.handle;
	bo->fd =drmfd;
	bo->active = 0;
	bo->count = 1;

	bo->memtype = DRM_KGSL_GEM_TYPE_FD_FBMEM;

	return bo;
}
