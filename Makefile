CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -O2 -lpcre

ALL = cbot
all: $(ALL)
clean:
	rm -f $(ALL)
