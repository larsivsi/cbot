CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -O2 -lpcre

ALL = cbot
all: $(ALL)
cbot: main.c
	$(CC) $(CFLAGS) main.c -o cbot
clean:
	rm -f $(ALL)
