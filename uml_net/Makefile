OBJS = uml_net.o
BIN = uml_net
TUNTAP = $(shell [ -e /usr/include/linux/if_tun.h ] && echo -DTUNTAP)
CFLAGS = -g -Wall $(TUNTAP)

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)/usr/bin
	install -s -m 04755 $(BIN) $(DESTDIR)/usr/bin
