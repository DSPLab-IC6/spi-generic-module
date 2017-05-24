.PHONY: all clean install uninstall

SYSROOT := $(HOME)/sysroots/staging/beaglebone-black/
KDIR ?= ${HOME}/sysroots/staging/beaglebone-black/lib/modules/4.4.23/build
PWD := $(shell pwd)
BUILD_DIR := $(PWD)/build
export ARCH := arm
export CROSS_COMPILE := armv7-bbb-linux-gnueabihf-

default: $(BUILD_DIR)
	touch $(BUILD_DIR)/Makefile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) modules

$(BUILD_DIR):
	mkdir -p "$@"

clean:
	${MAKE} -C ${KDIR} M=$(BUILD_DIR) src=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) SUBDIRS=$(BUILD_DIR) INSTALL_MOD_PATH=${SYSROOT} \
		modules_install
	install ${TARGET}.h  ${SYSROOT}/${PREFIX_HEADER}
#	install ${TARGET}.ko ${SYSROOT}/${PREFIX_KERNEL_MODULE}

uninstall:
	rm -f ${SYSROOT}/${PREFIX_KERNEL_MODULE}/${TARGET}.ko
	rm -f ${SYSROOT}/${PREFIX_HEADER}/${TARGER}.h

deploy:
	./deploy_via_scp.sh
