BIN = uml_moo
OBJS = $(BIN).o
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64

BIN_DIR ?= /usr/bin

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)$(BIN_DIR)
	install -s $(BIN) $(DESTDIR)$(BIN_DIR)
