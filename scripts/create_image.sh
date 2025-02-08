#!/bin/bash

# Check if root
if [[ "$EUID" > 0 ]]; then 
        echo "You must be root."
        exit -1
fi

# Create a loopback device for the disk image
LOOP_DEVICE=$(losetup --find --show $DISK_IMAGE_NAME) || exit -1
test -b $LOOP_DEVICE && echo "Created loopback device $LOOP_DEVICE"

# Create a FAT32 file system on the loopback device
sudo mkfs.vfat -F 32 $LOOP_DEVICE || exit -1
echo "Successfully created FAT32 file system on $LOOP_DEVICE"

# Mount the disk image and copy the memtest binary to it
mkdir $DISK_MOUNT_POINT 2> /dev/null
mount $DISK_IMAGE_NAME $DISK_MOUNT_POINT || exit -1
mkdir -p $EFI_PATH 2> /dev/null
cp -v ../build64/memtest.efi $EFI_PATH/BOOTX64.EFI
umount $DISK_MOUNT_POINT || exit -1
losetup -d $LOOP_DEVICE || exit -1
