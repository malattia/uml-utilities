BIN := uml_moo uml_mkcow

OBJS_uml_moo   := uml_moo.o cow.o
OBJS_uml_mkcow := uml_mkcow.o cow.o

override CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

$(OBJS): cow.h cow_sys.h

include ../config.mk
