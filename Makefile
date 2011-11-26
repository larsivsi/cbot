CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -O2

ALL = cbot
all: $(ALL)
clean:
	rm -f $(ALL)
