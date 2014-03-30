#!/bin/bash

if [ ! -d build ] ; then
        ./build_libs.sh
fi

cd build
MYPWD=${PWD}

export CFLAGS=-I${MYPWD}/include
export LDFLAGS=-L${MYPWD}/lib

gcc -O2 ${CFLAGS} ${LDFLAGS} -lzmq -lczmq ../my/broker.c -o bin/broker
strip -s bin/broker
