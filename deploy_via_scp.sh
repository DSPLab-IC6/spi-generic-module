#!/bin/sh

DEFAULT_UNAME=root
DEFAULT_IP=192.168.7.2

DEFAULT_TARGET='spi-protocol-generic'

DEFAULT_BUILD_DIR=`pwd`/build

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
TARGET_KERNEL_VERSION=$(ssh ${UNAME}@${IP} 'uname -r')
BUILD_DIR=${BUILD_DIR:=${DEFAULT_BUILD_DIR}}
PREFIX_KERNEL_MODULE=/lib/modules/${TARGET_KERNEL_VERSION}/extra
PREFIX_HEADER=${PREFIX_HEADER:=${DEFAULT_PREFIX_HEADER}}

scp ${BUILD_DIR}/${TARGET}.ko ${UNAME}@${IP}:${PREFIX_KERNEL_MODULE}
scp              ${TARGET}.h  ${UNAME}@${IP}:${PREFIX_HEADER}

