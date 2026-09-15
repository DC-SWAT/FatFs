# KallistiOS ##version##
#
# libfatfs Makefile
# (C) 2025-2026 Ruslan Rostovtsev
#

TARGET = libfatfs.a
OBJS = src/ffunicode.o \
		src/ff.o \
		src/dc.o \
		src/dc_bdev.o

KOS_CFLAGS += -W -Wextra -pedantic -Isrc -I../include

# Enable debug output if DEBUG=1
ifdef DEBUG
    KOS_CFLAGS += -DFATFS_DEBUG=1
endif

# Enable DMA buffer unless DMA_BUF=0
ifndef DMA_BUF
    DMA_BUF = 1
endif
ifeq ($(DMA_BUF), 1)
    KOS_CFLAGS += -DFATFS_USE_DMA_BUF=1
endif

# Enable CRC checking for SD cards if SD_CHECK_CRC=1
ifdef SD_CHECK_CRC
    KOS_CFLAGS += -DFATFS_SD_CHECK_CRC=1
endif

# exFAT + 64-bit LBA on by default; EXFAT=0 disables both
ifndef EXFAT
    EXFAT = 1
endif
ifeq ($(EXFAT), 0)
    KOS_CFLAGS += -DFF_FS_EXFAT=0 -DFF_LBA64=0
endif

include $(KOS_BASE)/addons/Makefile.prefab
