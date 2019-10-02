


CC = gcc
CFLAGS = -Wall -pedantic -std=gnu99
DEBUG = -g
TARGETS = 2310serv 2310client

.DEFAULT: all

.PHONY: all debug clean

all: $(TARGETS)

debug: CFLAGS += $(DEBUG)
debug: clean $(TARGETS)

shared.o: shared.c shared.h
	$(CC) $(CFLAGS) -c shared.c -o shared.o

2310client: 2310client.c shared.o
	$(CC) $(CFLAGS) 2310client.c shared.o -o 2310client

2310serv: 2310serv.c shared.o
	$(CC) $(CFLAGS) -pthread 2310serv.c shared.o -o 2310serv

clean:
	rm -f $(TARGETS) *.o
