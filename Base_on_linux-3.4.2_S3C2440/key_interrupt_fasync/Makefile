KERN_DIR = /home/lc_sagacity/work/arm/kernel/linux-3.4.2

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= key_interrupt_fasync.o
