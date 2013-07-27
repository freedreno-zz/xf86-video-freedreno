freedreno_drv_la_LIBADD =

AM_CFLAGS = @XORG_CFLAGS@ -Wall -Werror -I$(top_srcdir)/system-includes/ -I$(top_builddir)/

freedreno_drv_la_LTLIBRARIES = freedreno_drv.la
freedreno_drv_la_LDFLAGS = -module -avoid-version
freedreno_drv_la_LIBADD = @XORG_LIBS@
freedreno_drv_ladir = @moduledir@/drivers

freedreno_drv_la_SOURCES = \
	drmmode_display.c \
	fbmode_display.c \
	msm-driver.c \
	msm-accel.c \
	msm-exa.c \
	msm-dri2.c \
	msm-pixmap.c

EXTRA_DIST = \
	msm.h \
	msm-drm.h
