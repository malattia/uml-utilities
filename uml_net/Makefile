OBJS = uml_net.o
TUNTAP = $(shell [ -e /usr/include/linux/if_tun.h ] && echo -DTUNTAP)
CFLAGS = -g -Wall $(TUNTAP)

all : uml_net

uml_net : $(OBJS)
	$(CC) $(CFLAGS) -o uml_net $(OBJS)

clean : 
	rm -f uml_net $(OBJS) *~

install :
	install -d $(DESTDIR)/usr/bin
	install -s -m 04755 uml_net $(DESTDIR)/usr/bin
