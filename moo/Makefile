BIN = uml_moo
OBJS = $(BIN).o
CFLAGS = -g -Wall -D_LARGEFILE64_SOURCE

BIN_DIR = /usr/bin
LIB_DIR = /usr/lib/uml

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)$(BIN_DIR)
	install -s $(BIN) $(DESTDIR)$(BIN_DIR)
