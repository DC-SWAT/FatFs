# KallistiOS ##version##
#
# libfatfs Makefile
# (C) 2025 Ruslan Rostovtsev
#

TARGET = libfatfs.a
OBJS = src/ff.o
KOS_CFLAGS += -Isrc -Iinclude

include $(KOS_BASE)/addons/Makefile.prefab
