#!/bin/bash

if [ ! -d build ] ; then
	mkdir build
fi

cd build
MYPWD=${PWD}

function get_from_git()
{
	gitpath="";

	echo "--- "${1}" ---";
	case ${1} in
		czmq)  gitpath="https://github.com/zeromq/czmq.git"
			;;
		zeromq)  gitpath="https://github.com/zeromq/zeromq4-x.git"
			;;
		libsodium)  gitpath="https://github.com/jedisct1/libsodium.git"
			;;
		*) echo "Not Valid path"
			exit 1
			;;
	esac

	if [ ! -d ${1} ] ; then
		git clone ${gitpath} ${1}
	else
		cd ${1} && \
		git pull && \
		cd ..
	fi

	if [ $? -ne 0 ] ; then
		echo "git error"
		exit 1
	fi
}

function compile()
{
	echo "--- "${1}" ---";
	cd ${1} && \
	./autogen.sh && \
	./configure --prefix=${MYPWD} && \
	make -j5 && \
	make install && \
	cd ..

	if [ $? -ne 0 ] ; then
		echo "compile error"
		exit 1
	fi
}

echo "------------------ GIT ------------------";

get_from_git zeromq
get_from_git libsodium
get_from_git czmq

echo "------------------ Compile ------------------";

export CFLAGS=-I${MYPWD}/include
export LDFLAGS=-L${MYPWD}/lib

compile zeromq
compile libsodium
compile czmq

cd ..

