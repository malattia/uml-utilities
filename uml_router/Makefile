OBJS = uml_switch.o
BIN = uml_switch
CFLAGS = -g -Wall

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)/usr/bin
	install -s $(BIN) $(DESTDIR)/usr/bin
