PROGRAM = i3blocks
CC=cc
CFLAGS=-Wall

debug: CFLAGS+=-DDEBUG -g

re-search: re-search.c
	$(CC) $(CFLAGS) $^ -o $@

debug: re-search

all: re-search

clean:
	rm -f re-search
