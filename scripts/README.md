```build.sh``` creates an empty disk image for QEMU to use.
The build script invokes ```create_image.sh``` which mounts the disk image
file as a loopback device which can then be used to create a file system. 
It mounts this device on a local directory and copies the memtest binary over.

```run.sh``` starts QEMU waiting for GDB on the newly created disk image.

