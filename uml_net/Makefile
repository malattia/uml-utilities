OBJS = uml_net.o
CFLAGS = -g -Wall

all : uml_net

uml_net : $(OBJS)
	$(CC) $(CFLAGS) -o uml_net $(OBJS)

clean : 
	rm -f uml_net $(OBJS) *~

install :
	install uml_net /usr/bin
