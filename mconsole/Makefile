BIN = uml_mconsole
OBJS = $(BIN).o
CFLAGS = -g -Wall

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS) -lreadline -lncurses

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)/usr/bin
	install -s $(BIN) $(DESTDIR)/usr/bin
