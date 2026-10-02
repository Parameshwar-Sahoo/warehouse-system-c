obj-m += wms_driver.o

DEFAULT_KDIR := /lib/modules/$(shell uname -r)/build
GENERIC_KDIR := $(wildcard /usr/src/linux-headers-*-generic)

ifeq ($(wildcard $(DEFAULT_KDIR)),)
    KDIR ?= $(firstword $(GENERIC_KDIR))
else
    KDIR ?= $(DEFAULT_KDIR)
endif

PWD := $(shell pwd)
EXTRA_CFLAGS += -I$(PWD)/include

all:
	@if [ -z "$(KDIR)" ] || [ ! -d "$(KDIR)" ]; then \
		echo "ERROR: Kernel build directory not found. Please install linux-headers or set KDIR manually."; \
		exit 1; \
	fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@if [ -n "$(KDIR)" ] && [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) clean; \
	else \
		rm -f *.o *.ko *.mod *.mod.c *.order *.symvers .*.cmd; \
		rm -rf .tmp_versions; \
	fi

install:
	sudo insmod wms_driver.ko
	sudo chmod 666 /dev/wms_driver

uninstall:
	sudo rmmod wms_driver || true

.PHONY: all clean install uninstall
