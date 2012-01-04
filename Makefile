CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -O2 -g -I/usr/include/postgresql
LDFLAGS+=-lpcre -lpthread -lcurl -lpq

ALL = cbot
all: $(ALL)

cbot: main.o config.o title.o log.o eightball.o timer.o
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

config.o: config.c
	$(CC) $(CFLAGS) -c -o $@ $<

title.o: title.c
	$(CC) $(CFLAGS) -c -o $@ $<

log.o: log.c
	$(CC) $(CFLAGS) -c -o $@ $<

eightball.o: eightball.c
	$(CC) $(CFLAGS) -c -o $@ $<

timer.o: timer.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(ALL) main.o config.o title.o log.o eightball.o
