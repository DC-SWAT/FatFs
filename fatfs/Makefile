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

KOS_CFLAGS += -W -Wextra -pedantic -Isrc -Iinclude

include $(KOS_BASE)/addons/Makefile.prefab
