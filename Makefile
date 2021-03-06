CONFIG_MODULE_SIG=n
ifneq ($(KERNELRELEASE),)
obj-m := myfs.o
else
KDIR:=/lib/modules/$(shell uname -r)/build
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order
endif

install:
	sudo insmod myfs.ko
uninstall:
	sudo rmmod myfs
mount:
	mkdir testmp && sudo mount -t my none ./testmp
umount:
	sudo umount ./testmp && rm -rf ./testmp