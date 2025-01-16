FatFs for the Sega Dreamcast
==========

FatFs is a generic FAT file system module for small embedded systems. This version has been ported to the Sega Dreamcast platform to provide seamless support for FAT and FAT32 file systems on the console. It has been specifically optimized to leverage the hardware capabilities of the Dreamcast, ensuring efficient file operations, compatibility with the system's architecture, and reliable performance for various storage solutions. The initial FatFs implementation for KallistiOS was taken from DreamShell.

## Build
```console
make
```

## Usage
You only need to call `fs_fat_mount_sd()` and/or `fs_fat_mount_ide()` to automatically mount all FAT partitions on the corresponding devices. Additionally, you can use other block devices by invoking `fs_fat_mount()` with the appropriate parameters for the target device.

## Links
- DreamShell: https://github.com/DC-SWAT/DreamShell
- KallistiOS: https://github.com/KallistiOS/KallistiOS
- FatFs: http://elm-chan.org/fsw/ff/
