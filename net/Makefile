CC = gcc
CFLAGS = -Wall -ggdb
UTIL_OBJS = um_eth_net_util.o um_eth_net_output.o um_eth_net_phy.o um_eth_net_tap.o

all: um_eth_net_util um_eth_net_set

um_eth_net_phy.o: um_eth_net_phy.c um_eth_net.h
um_eth_net_tap.o: um_eth_net_tap.c um_eth_net.h
um_eth_net_util.o: um_eth_net_util.c um_eth_net.h
um_eth_net_set.o: um_eth_net_set.c um_eth_net.h

um_eth_net_util: $(UTIL_OBJS)
	gcc $(CFLAGS) -o $@ $(UTIL_OBJS)

um_eth_net_set: um_eth_net_set.o
	gcc $(CFLAGS) -o $@ $<

dist:
	cd .. ; tar --exclude=CVS -zcf um_eth_net-0.005.tar.gz net/

clean:
	rm -rf um_eth_net_util um_eth_net_set *.o core
