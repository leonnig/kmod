obj-m += hello.o
obj-m += hello-2.o
obj-m += hello-5.o
obj-m += chardev.o
obj-m += procfs.o
obj-m += seq.o

PWD := $(shell pwd)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod
