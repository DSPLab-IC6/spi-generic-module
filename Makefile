# Makefile – makefile of our first driver

ifneq (${KERNELRELEASE},)

obj-m := spi-protocol-generic.o

else

KDIR := ${HOME}/sysroots/staging/beaglebone-black/lib/modules/4.4.23/build
PWD := $(shell pwd)
export ARCH := arm
export CROSS_COMPILE := armv7-bbb-linux-gnueabihf-

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	${MAKE} -C ${KDIR} SUBDIRS=${PWD} clean

endif
