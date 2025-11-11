# Makefile for pci_bar_dump kernel module

obj-m += pci_bar_dump.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod pci_bar_dump.ko || true

remove:
	sudo rmmod pci_bar_dump || true

info:
	modinfo pci_bar_dump.ko
