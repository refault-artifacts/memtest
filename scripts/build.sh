#!/bin/bash

DISK_IMAGE_SIZE=100M
DISK_IMAGE_NAME=../build64/disk.img
DISK_MOUNT_POINT=../build64/mnt
EFI_PATH=$DISK_MOUNT_POINT/EFI/BOOT

# Use the memtest-provided makefile to build memtest
cd ../build64
make || exit -1
cd ../scripts

# Create an empty disk image
dd if=/dev/zero of=$DISK_IMAGE_NAME bs=$DISK_IMAGE_SIZE count=1 \
2> /dev/null || exit -1
test -f $DISK_IMAGE_NAME && echo "Successfully created empty disk image \
$DISK_IMAGE_NAME"

echo "############################################"
echo "# To use a loopback device like /dev/loop0 #"
echo "# you must be root. Please enter your sudo #"
echo "# passphrase below if asked.               #"
echo "############################################"
export DISK_IMAGE_NAME=$DISK_IMAGE_NAME
export DISK_MOUNT_POINT=$DISK_MOUNT_POINT
export EFI_PATH=$EFI_PATH
sudo -E ./create_image.sh || exit -1

./run.sh





