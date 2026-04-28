obj-m += monitor.o

all:
	gcc engine.c -o engine -lpthread
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -f engine
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
