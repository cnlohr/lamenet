all : lamenet

CFLAGS:=$(CFLAGS) -g -static
CXXFLAGS:=$(CFLAGS)
CC_MIPS:=/win/crosscompiler/toolchain-mips_34kc_gcc-5.3.0_musl-1.1.14/bin/mips-openwrt-linux-gcc
export STAGING_DIR = ..

lamenet : lamenet.c librawp.c
	$(CC)  $(CFLAGS) -o $@ $^ -s -Os -lpthread

lamenet.openwrtmips : lamenet.c librawp.c
	$(CC_MIPS)  $(CFLAGS) -o $@ $^ -s -Os

pairrun : lamenet.openwrtmips
	cat lamenet.openwrtmips | ssh root@192.168.1.1 "killall lamenet; cat - > /tmp/lamenet; chmod +x /tmp/lamenet; cd /tmp; ./lamenet eth0 eth1 -p 100 -a 1 -t 1 -b 1000 -d 1 -l test.pcap"

clean : 
	rm -rf *.o *~ lamenet lamenet.openwrtmips
