ifneq ($(KERNELRELEASE),)
obj-m := mysht30.o
else
SRC := $(shell pwd)

# 編譯出全部的.ko檔案
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

# 安裝模組
modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

# 清除模組
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
endif