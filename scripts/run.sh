#!/bin/bash

DISK_IMAGE_NAME=../build64/disk.img

echo "Starting QEMU, waiting for GDB to connect on port 1234"
echo "############### Run 'gdb -x gdbfile' #################"
qemu-system-x86_64 -s -S -cpu max -smp 4 \
-bios /usr/share/edk2-ovmf/OVMF_CODE.fd \
-drive file=$DISK_IMAGE_NAME,if=ide,format=raw -net none -m 12G \
-device qemu-xhci,id=xhci \
-device usb-host,bus=xhci.0,port=1,vendorid=5824,productid=1158 &
#-device usb-hub,bus=xhci.0,port=1 \
#-device usb-kbd,bus=xhci.0,port=1.1 \
#-device usb-kbd,bus=xhci.0,port=1.3 \

