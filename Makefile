CC=gcc
CFLAGS=-Wall

debug: CFLAGS+=-DDEBUG -g

re-search: re-search.c
	$(CC) re-search.c -o $@ $(CFLAGS)

debug: re-search

all: re-search

clean:
	rm -f re-search