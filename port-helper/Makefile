OBJS = port-helper.o
BIN = port-helper
CFLAGS = -g -Wall

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)/usr/lib/uml
	install -s $(BIN) $(DESTDIR)/usr/lib/uml
