#!/bin/sh

DEFAULT_UNAME=root
DEFAULT_IP=192.168.7.2

DEFAULT_TARGET='spi-generic-module'

DEFAULT_BUILD_DIR=`pwd`/build

DEFAULT_PREFIX_KERNEL_MODULE="/lib/modules/'`uname -r`'/extra"
DEFAULT_PREFIX_HEADER="/usr/include"

UNAME=${UNAME:=${DEFAULT_UNAME}}
IP=${IP:=${DEFAULT_IP}}

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix-module)
      PREFIX_KERNEL_MODULE=shift
      ;;
    --prefix-herader)
      PREFIX_HEADER=shift
      ;;
    --target)
      TARGET=shift
      ;;
		--build-dir)
			BUILD_DIR=shift
			;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument *\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

TARGET=${TARGET:=${DEFAULT_TARGET}}
BUILD_DIR=${BUILD_DIR:=${DEFAULT_BUILD_DIR}}
PREFIX_KERNEL_MODULE=${PREFIX_KERNEL_MODULE:=${DEFAULT_PREFIX_KERNEL_MODULE}}
PREFIX_HEADER=${PREFIX_HEADER:=${DEFAULT_PREFIX_HEADER}}

scp ${BUILD_DIR}/${TARGET}.ko ${UNAME}@${IP}:${PREFIX_KERNEL_MODULE}
scp 						 ${TARGET}.h  ${UNAME}@${IP}:${PREFIX_HEADER}

