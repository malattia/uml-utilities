BIN := uml_mount
# I have the FUSE headers in /usr/local/include
override CFLAGS += -D_FILE_OFFSET_BITS=64 -I/usr/local/include
LIBS := -lfuse

include ../config.mk
