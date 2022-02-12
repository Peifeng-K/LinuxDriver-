

ROOTFS_DIR = /home/linux/source/rootfs
MODULE_NAME=led_drv_plus
APP_NAME = led_app

#CROSS_COMPILE = arm-none-linux-gnueabi-
CROSS_COMPILE = arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc

ifeq ($(KERNELRELEASE), )


KERNEL_DIR = /home/linux/arm_development/iMX6ULL/linux-imx-rel_imx_4.1.15_2.1.0_ga/
CUR_DIR = $(shell pwd)
all :
	make -C  $(KERNEL_DIR) M=$(CUR_DIR) modules
	$(CC) $(APP_NAME).c  -o $(APP_NAME)

clean :
	make -C  $(KERNEL_DIR) M=$(CUR_DIR) clean
	rm -rf $(APP_NAME)
install:
	cp -raf *.ko $(APP_NAME)   $(ROOTFS_DIR)/drv_module
#	cp -raf *.ko   $(ROOTFS_DIR)/drv_module


else

obj-m += $(MODULE_NAME).o


endif
