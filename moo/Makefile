BIN = uml_moo
OBJS = $(BIN).o
CFLAGS = -g -Wall -D_LARGEFILE64_SOURCE

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install :
	install -d $(DESTDIR)/usr/bin
	install -s $(BIN) $(DESTDIR)/usr/bin
