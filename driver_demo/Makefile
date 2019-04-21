obj-m := driver.o
MAKEOPT := \
	ARCH="arm" \
	CROSS_COMPILE="arm-none-linux-gnueabi-"

ARM_LINUX_KERNEL := "/home/liuw/svn/Prolin/firmware_3/build_dir/linux-brcm5830_sxxx-prolin2/linux-3.6.5/"

PWD = $(shell pwd)

all:
	$(MAKE) $(MAKEOPT) -C $(ARM_LINUX_KERNEL) M=$(PWD) modules

clean:
	rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions Module.symvers .Makefile.swp modules.order

















