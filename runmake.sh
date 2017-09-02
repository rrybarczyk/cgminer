#!/bin/sh

make clean
make distclean

./autogen.sh

ROOTFS_DIR=/home/Users/pengp/workspace/rootfs
MAKE_JOBS=16

LDFLAGS=-L${ROOTFS_DIR}/lib \
CFLAGS=-I${ROOTFS_DIR}/include \
./configure --prefix=${ROOTFS_DIR} \
--enable-bitmine_A1 --without-curses --host=arm-xilinx-linux-gnueabi --build=x86_64-pc-linux-gnu # --target=arm

make -j${MAKE_JOBS}

cp ./cgminer /home/public/update/cgminer_pp.$1
chmod 777 /home/public/update/cgminer_pp.$1

