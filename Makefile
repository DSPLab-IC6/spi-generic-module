# Makefile – makefile of our first driver

TARGET = spi-generic-module

.PHONY: all clean install uninstall

BUILD_DIR ?= $(PWD)/build
KDIR := ${HOME}/sysroots/staging/beaglebone-black/lib/modules/4.4.23/build
PWD := $(shell pwd)
export ARCH := arm
export CROSS_COMPILE := armv7-bbb-linux-gnueabihf-

all: $(TARGET)

$(TARGET): $(BUILD_DIR)
	$(MAKE) -C $(KDIR) SUBDIRS=$(BUILD_DIR) src=$(PWD) modules

$(BUILD_DIR):
	mkdir -p "$@"

clean:
	${MAKE} -C ${KDIR} SUBDIRS=${BUILD_DIR} src=$(PWD) clean

install:
	install ${TARGET}.ko ${SYSROOT}/${PREFIX_KERNEL_MODULE}
	install ${TARGET}.h  ${SYSROOT}/${PREFIX_HEADER}

uninstall:
	rm -f ${SYSROOT}/${PREFIX_KERNEL_MODULE}/${TARGET}.ko
	rm -f ${SYSROOT}/${PREFIX_HEADER}/${TARGER}.h

deploy:
	./deploy_via_scp.sh

