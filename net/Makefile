CC = gcc

# 2.4 kernels should include -DTUNTAP first
# 2.2 kernel (and some 2.4 kernel) should leave off -DTUNTAP
CFLAGS = -Wall -ggdb

all: um_eth_serv um_eth_tool

SERV_OBJS = phy.o tap.o input.o um_eth_serv.o output.o

um_eth_serv: $(SERV_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERV_OBJS)

um_eth_tool: um_eth_tool.o
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf um_eth_serv um_eth_tool *.o core *~
