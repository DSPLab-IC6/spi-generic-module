# Makefile – makefile of our first driver
 
# if KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq (${KERNELRELEASE},)
    obj-m := spi-protocol-generic.o
# Otherwise we were called directly from the command line.
# Invoke the kernel build system.
else
KDIR := ${HOME}/sysroots/staging/beaglebone-black/lib/modules/4.4.23/build
export ARCH := arm
export CROSS_COMPILE := armv7-bbb-linux-gnueabihf-
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
 
clean:
	${MAKE} -C ${KDIR} SUBDIRS=${PWD} clean
endif
