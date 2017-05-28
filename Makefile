.PHONY: all clean install uninstall

TARGET := "spi-protocol-generic"

PREFIX_HEADER := usr/include

SYSROOT := $(HOME)/sysroots/staging/beaglebone-black
KDIR ?= ${HOME}/sysroots/staging/beaglebone-black/lib/modules/4.4.23/build
PWD := $(shell pwd)
BUILD_DIR := $(PWD)/build
export ARCH := arm
export CROSS_COMPILE := armv7-bbb-linux-gnueabihf-

default: $(BUILD_DIR)
	touch $(BUILD_DIR)/Makefile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) modules

debug: $(BUILD_DIR)
	$(MAKE) CFLAGS_MODULE="-DDEBUG_SPI_PROTOCOL_GENERIC" default

$(BUILD_DIR):
	mkdir -p "$@"

clean:
	${MAKE} -C ${KDIR} M=$(BUILD_DIR) src=$(PWD) clean

install: $(PREFIX_HEADER)
	$(MAKE) -C $(KDIR) SUBDIRS=$(BUILD_DIR) INSTALL_MOD_PATH=${SYSROOT} \
		modules_install
	install ${TARGET}.h  ${SYSROOT}/${PREFIX_HEADER}

$(PREFIX_HEADER):
	mkdir -p $(SYSROOT)/"$@"

uninstall:
	rm -f ${SYSROOT}/${PREFIX_KERNEL_MODULE}/${TARGET}.ko
	rm -f ${SYSROOT}/${PREFIX_HEADER}/${TARGER}.h

deploy:
	./deploy-via-scp.sh
