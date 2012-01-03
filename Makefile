CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -O2 -g
LDFLAGS+=-lpcre -lpthread -lcurl 

ALL = cbot
all: $(ALL)

cbot: main.o config.o title.o
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

config.o: config.c
	$(CC) $(CFLAGS) -c -o $@ $<

title.o: title.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(ALL) main.o config.o title.o
