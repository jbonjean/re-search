CC=cc
CFLAGS=-Wall -O2

debug: CFLAGS=-DDEBUG -g -Wall

re-search: re-search.c
	$(CC) $(CFLAGS) $^ -o $@

debug: re-search

all: re-search

clean:
	rm -f re-search
