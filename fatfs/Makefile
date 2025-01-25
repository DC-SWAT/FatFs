# KallistiOS ##version##
#
# libfatfs Makefile
# (C) 2025 Ruslan Rostovtsev
#

TARGET = libfatfs.a
OBJS = src/option/ccsbcs.o \
		src/option/syscall.o \
		src/ff.o \
		src/dc.o \
		src/dc_bdev.o

KOS_CFLAGS += -W -Wextra -pedantic -Isrc -I../include

# Uncomment to enable debug output.
# KOS_CFLAGS += -DFATFS_DEBUG=1

# Comment out to reduce memory usage.
KOS_CFLAGS += -DFATFS_USE_DMA_BUF=1

include $(KOS_BASE)/addons/Makefile.prefab
