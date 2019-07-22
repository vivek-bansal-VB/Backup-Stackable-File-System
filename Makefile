BKPFS_VERSION="0.1"

EXTRA_CFLAGS += -DBKPFS_VERSION=\"$(BKPFS_VERSION)\"

obj-$(CONFIG_BKP_FS) += ../fs/bkpfs/

all: bkpfs bkpctl

bkpfs-y := ../fs/bkpfs/dentry.o ../fs/bkpfs/file.o ../fs/bkpfs/inode.o ../fs/bkpfs/main.o ../fs/bkpfs/super.o ../fs/bkpfs/lookup.o ../fs/bkpfs/mmap.o

bkpfs:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

bkpctl: bkpctl.c
	gcc -Wall -Werror bkpctl.c -o bkpctl 
        
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f ../fs/bkpfs/*.ko
	rm -f ../fs/bkpfs/*.o
