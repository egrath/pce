# Makefile

CC = gcc
LD = gcc

CFLAGS = -Os -Wall -D_FILE_OFFSET_BITS=64

SRC := pbicat.c
OBJ := pbicat.o
BIN := pbicat

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)

pbicat: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

pbicat.o: pbicat.c
