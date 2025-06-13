obj-m += pci_bar_dump.o

all:
	make -C /lib/modules/6.6.92.2-1.azl3/build M=$(PWD) modules

clean:
	make -C /lib/modules/6.6.92.2-1.azl3/build M=$(PWD) clean
