/*
 * FatFs for the Sega Dreamcast
 *
 * This file is part of the FatFs module, a generic FAT filesystem
 * module for small embedded systems. This version has been ported and
 * optimized specifically for the Sega Dreamcast platform.
 *
 * Copyright (c) 2007-2025 Ruslan Rostovtsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <malloc.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <dc/g1ata.h>
#include <dc/sd.h>
#include <dc/scif.h>
#include <kos/dbglog.h>
#include <fatfs.h>

#include "ffconf.h"

#define MAX_PARTITIONS 4

static kos_blockdev_t *sd_dev = NULL;
static kos_blockdev_t *g1_dev = NULL;
static kos_blockdev_t *g1_dev_dma = NULL;

static int is_fat_partition(uint8_t partition_type) {
    switch(partition_type) {
        case 0x04: /* 32MB */
        case 0x06: /* Over 32 to 2GB */
            return 16;
        case 0x0B:
        case 0x0C:
            return 32;
#if FF_FS_EXFAT
        case 0x07: /* Microsoft / exFAT (also NTFS) */
            return 64;
#endif
        default:
            return 0;
    }
}

static const char *fat_fs_name(int fat_part) {
    switch (fat_part) {
        case 16:
            return "FAT16";
        case 32:
            return "FAT32";
        case 64:
            return "exFAT";
        default:
            return "FAT";
    }
}

static int check_partition(uint8_t *buf, int partition) {
    int pval;
    
    if (buf[0x01FE] != 0x55 || buf[0x1FF] != 0xAA) {
        // dbglog(DBG_DEBUG, "Device doesn't appear to have a MBR\n");
        return -1;
    }
    
    pval = 16 * partition + 0x01BE;

    if (buf[pval + 4] == 0) {
        // dbglog(DBG_DEBUG, "Partition empty: 0x%02x\n", buf[pval + 4]);
        return -1;
    }
    
    return 0;
}

#if FF_LBA64
static bool mbr_is_gpt(const uint8_t *buf) {
    if (buf[0x01FE] != 0x55 || buf[0x1FF] != 0xAA) {
        return false;
    }
    return buf[0x01BE + 4] == 0xEE;
}
#endif

static void make_mount_path(char *path, const char *prefix, int part) {
    if (part == 0) {
        strcpy(path, prefix);
    }
    else {
        sprintf(path, "%s%d", prefix, part);
    }
}

static bool mount_sd_card(uint8_t *mbr_buf) {
    uint8_t partition_type;
    int part = 0, fat_part = 0;
    char path[8];
    kos_blockdev_t *dev;
    const char *prefix = "/sd";
    bool mounted = false;

    if (sd_dev == NULL) {
        sd_dev = malloc(sizeof(kos_blockdev_t) * MAX_PARTITIONS);

        if (sd_dev == NULL) {
            dbglog(DBG_ERROR, "FATFS: Can't allocate memory for SD card partitions\n");
            return false;
        }

        memset(sd_dev, 0, sizeof(kos_blockdev_t) * MAX_PARTITIONS);
    }

#if FF_LBA64
    if (mbr_is_gpt(mbr_buf)) {
        dbglog(DBG_INFO, "FATFS: Detected GPT on SD card\n");
        for (part = 0; part < MAX_PARTITIONS; part++) {
            dev = &sd_dev[part];
            if (sd_blockdev_for_device(dev)) {
                if (part == 0) {
                    return false;
                }
                break;
            }
            make_mount_path(path, prefix, part);
            if (fs_fat_init()) {
                dbglog(DBG_INFO, "FATFS: Could not initialize fatfs!\n");
                dev->shutdown(dev);
                return mounted;
            }
            dbglog(DBG_INFO, "FATFS: Mounting GPT volume to %s...\n", path);
            if (fs_fat_mount(path, dev, NULL, part)) {
                dbglog(DBG_INFO, "FATFS: Could not mount GPT volume %d.\n", part);
                dev->shutdown(dev);
                break;
            }
            mounted = true;
        }
        return mounted;
    }
#endif

    for (part = 0; part < MAX_PARTITIONS; part++) {
        dev = &sd_dev[part];

        if (check_partition(mbr_buf, part)) {
            continue;
        }
        if (sd_blockdev_for_partition(part, dev, &partition_type)) {
            continue;
        }

        make_mount_path(path, prefix, part);

        /* Check to see if the MBR says that we have a FAT partition. */
        fat_part = is_fat_partition(partition_type);

        if (fat_part) {

            dbglog(DBG_INFO, "FATFS: Detected %s on partition %d\n", fat_fs_name(fat_part), part);

            if (fs_fat_init()) {
                dbglog(DBG_INFO, "FATFS: Could not initialize fatfs!\n");
                dev->shutdown(dev);
            }
            else {
                /* Need full disk block device for FAT */
                dev->shutdown(dev);

                if (sd_blockdev_for_device(dev)) {
                    continue;
                }

                dbglog(DBG_INFO, "FATFS: Mounting filesystem to %s...\n", path);

                if (fs_fat_mount(path, dev, NULL, part)) {
                    dbglog(DBG_INFO, "FATFS: Could not mount device as fatfs.\n");
                    dev->shutdown(dev);
                }
                else {
                    mounted = true;
                }
            }
        }
        else {
            dbglog(DBG_INFO, "FATFS: Unknown filesystem: 0x%02x\n", partition_type);
            dev->shutdown(dev);
        }
    }

    return mounted;
}

int fs_fat_mount_sd() {
    uint8_t mbr_buffer[512];
    sd_init_params_t params;

    dbglog(DBG_INFO, "FATFS: Checking for SD cards...\n");

    /* Try SCIF interface first */
    params.interface = SD_IF_SCIF;
#ifdef FATFS_SD_CHECK_CRC
    params.check_crc = true;
#else
    params.check_crc = false;
#endif

    memset(mbr_buffer, 0, sizeof(mbr_buffer));

    if (sd_init_ex(&params) == 0) {
        dbglog(DBG_INFO, "FATFS: SD card found on SCIF-SPI: %"PRIu32" MB\n",
            (uint32_t)(sd_get_size() / 1024 / 1024));

        if (sd_read_blocks(0, 1, mbr_buffer) == 0) {
            if (mount_sd_card(mbr_buffer)) {
                return 0;
            }
        }
        else {
            dbglog(DBG_ERROR, "FATFS: Can't read MBR from SCIF-SPI SD card\n");
        }
    }

    /* Get back dbglog if they are used */
    scif_init();

    /* If no card found on SCIF, try SCI interface */
    params.interface = SD_IF_SCI;

    if (sd_init_ex(&params) == 0) {
        dbglog(DBG_INFO, "FATFS: SD card found on SCI-SPI: %"PRIu32" MB\n",
            (uint32_t)(sd_get_size() / 1024 / 1024));

        if (sd_read_blocks(0, 1, mbr_buffer) == 0) {
            if (mount_sd_card(mbr_buffer)) {
                return 0;
            }
        }
        else {
            dbglog(DBG_ERROR, "FATFS: Can't read MBR from SCI-SPI SD card\n");
        }
    }
    /* No cards found on any interface */
    return -1;
}

int fs_fat_mount_ide() {

    uint8_t partition_type;
    int part = 0, fat_part = 0;
    char path[8];
    uint8_t buf[512];
    kos_blockdev_t *dev;
    kos_blockdev_t *dev_dma;

    dbglog(DBG_INFO, "FATFS: Checking for G1 ATA devices...\n");

    if (g1_ata_init()) {
        return -1;
    }

    /* Read the MBR from the disk */
    if (g1_ata_lba_mode()) {
        if (g1_ata_read_lba(0, 1, (uint16_t *)buf) < 0) {
            dbglog(DBG_ERROR, "FATFS: Can't read MBR from IDE by LBA\n");
            return -1;
        }
    }
    else {
        if (g1_ata_read_chs(0, 0, 1, 1, (uint16_t *)buf) < 0) {
            dbglog(DBG_ERROR, "FATFS: Can't read MBR from IDE by CHS\n");
            return -1;
        }
    }

    if (g1_dev == NULL) {
        g1_dev = malloc(sizeof(kos_blockdev_t) * MAX_PARTITIONS);
        g1_dev_dma = malloc(sizeof(kos_blockdev_t) * MAX_PARTITIONS);
    }
    if (!g1_dev || !g1_dev_dma) {
        dbglog(DBG_ERROR, "FATFS: Can't allocate memory for IDE partitions\n");
        return -1;
    }

    memset(&g1_dev[0], 0, sizeof(kos_blockdev_t) * MAX_PARTITIONS);
    memset(&g1_dev_dma[0], 0, sizeof(kos_blockdev_t) * MAX_PARTITIONS);

#if FF_LBA64
    if (mbr_is_gpt(buf)) {
        dbglog(DBG_INFO, "FATFS: Detected GPT on G1 ATA\n");
        for (part = 0; part < MAX_PARTITIONS; part++) {
            dev = &g1_dev[part];
            dev_dma = &g1_dev_dma[part];
            if (g1_ata_blockdev_for_device(0, dev)) {
                break;
            }
            if (g1_ata_blockdev_for_device(1, dev_dma)) {
                dev_dma = NULL;
            }
            make_mount_path(path, "/ide", part);
            if (fs_fat_init()) {
                dbglog(DBG_INFO, "FATFS: Could not initialize fatfs!\n");
                dev->shutdown(dev);
                if (dev_dma) {
                    dev_dma->shutdown(dev_dma);
                }
                return 0;
            }
            dbglog(DBG_INFO, "FATFS: Mounting GPT volume to %s...\n", path);
            if (fs_fat_mount(path, dev, dev_dma, part)) {
                dbglog(DBG_INFO, "FATFS: Could not mount GPT volume %d.\n", part);
                dev->shutdown(dev);
                if (dev_dma) {
                    dev_dma->shutdown(dev_dma);
                }
                break;
            }
        }
        return 0;
    }
#endif

    for (part = 0; part < MAX_PARTITIONS; part++) {

        dev = &g1_dev[part];
        dev_dma = &g1_dev_dma[part];

        if (check_partition(buf, part)) {
            continue;
        }
        if (g1_ata_blockdev_for_partition(part, 0, dev, &partition_type)) {
            continue;
        }

        make_mount_path(path, "/ide", part);

        /* Check to see if the MBR says that we have a FAT partition. */
        fat_part = is_fat_partition(partition_type);

        if (fat_part) {

            dbglog(DBG_INFO, "FATFS: Detected %s on partition %d\n", fat_fs_name(fat_part), part);

            if (fs_fat_init()) {
                dbglog(DBG_INFO, "FATFS: Could not initialize fatfs!\n");
                dev->shutdown(dev);
            }
            else {
                /* Need full disk block device for FAT */
                dev->shutdown(dev);

                if (g1_ata_blockdev_for_device(0, dev)) {
                    continue;
                }

                if (g1_ata_blockdev_for_device(1, dev_dma)) {
                    dev_dma = NULL;
                }

                dbglog(DBG_INFO, "FATFS: Mounting filesystem to %s...\n", path);

                if (fs_fat_mount(path, dev, dev_dma, part)) {
                    dbglog(DBG_INFO, "FATFS: Could not mount device as fatfs.\n");
                    dev->shutdown(dev);
                    if (dev_dma) {
                        dev_dma->shutdown(dev_dma);
                    }
                }
            }
        }
        else {
            dbglog(DBG_INFO, "FATFS: Unknown filesystem: 0x%02x\n", partition_type);
            dev->shutdown(dev);
        }
    }
    return 0;
}

/* Unmount and cleanup SD devices */
void fs_fat_unmount_sd(void) {
    if (sd_dev != NULL) {
        for (int i = 0; i < MAX_PARTITIONS; i++) {
            if (sd_dev[i].dev_data != NULL) {
                char path[16];
                if (i == 0) {
                    strcpy(path, "/sd");
                }
                else {
                    sprintf(path, "/sd%d", i);
                }
                if (fs_fat_unmount(path) < 0) {
                    sd_dev[i].shutdown(&sd_dev[i]);
                }
                sd_dev[i].dev_data = NULL;
            }
        }
        free(sd_dev);
        sd_dev = NULL;
    }

    sd_shutdown();
}

/* Unmount and cleanup IDE devices */
void fs_fat_unmount_ide(void) {
    if (g1_dev != NULL) {
        for (int i = 0; i < MAX_PARTITIONS; i++) {
            if (g1_dev[i].dev_data != NULL) {
                char path[16];
                if (i == 0) {
                    strcpy(path, "/ide");
                }
                else {
                    sprintf(path, "/ide%d", i);
                }
                if (fs_fat_unmount(path) < 0) {
                    g1_dev[i].shutdown(&g1_dev[i]);
                    if (g1_dev_dma != NULL && g1_dev_dma[i].dev_data != NULL) {
                        g1_dev_dma[i].shutdown(&g1_dev_dma[i]);
                    }
                }
                g1_dev[i].dev_data = NULL;
                if (g1_dev_dma != NULL) {
                    g1_dev_dma[i].dev_data = NULL;
                }
            }
        }
        free(g1_dev);
        g1_dev = NULL;
    }

    if (g1_dev_dma != NULL) {
        free(g1_dev_dma);
        g1_dev_dma = NULL;
    }

    g1_ata_shutdown();
}
