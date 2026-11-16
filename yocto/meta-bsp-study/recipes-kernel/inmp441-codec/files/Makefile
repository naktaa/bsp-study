obj-m := inmp441_codec.o

.PHONY: modules modules_install clean

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) modules_install

clean:
	rm -f *.o *.ko *.mod *.mod.c .*.cmd Module.symvers modules.order
