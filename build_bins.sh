#!/bin/bash

if [ ! -d build ] ; then
        ./build_libs.sh
fi

cd build
MYPWD=${PWD}

export CFLAGS=-I${MYPWD}/include
export LDFLAGS=-L${MYPWD}/lib

gcc -O2 ${CFLAGS} ${LDFLAGS} -lnanomsg -lmsgpack ../my/server.c -o bin/server && strip -s bin/server
echo "LD_LIBRARY_PATH=\""${MYPWD}/lib/"\" ./build/bin/server"
gcc -std=gnu11 -O2 ${CFLAGS} ${LDFLAGS} -lnanomsg -lmsgpack ../my/client.c -pthread -lglib-2.0 -I/usr/include/glib-2.0/ -I/usr/lib64/glib-2.0/include/ -o bin/client && strip -s bin/client
echo "LD_LIBRARY_PATH=\""${MYPWD}/lib/"\" ./build/bin/client"

