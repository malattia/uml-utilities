BIN = uml_mconsole
OBJS = $(BIN).o
CFLAGS = -g -Wall

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS) -lreadline

clean : 
	rm -f $(BIN) $(OBJS) *~

install :
	install $(BIN) /usr/bin
