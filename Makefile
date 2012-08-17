OBJECTS=main.o config.o title.o log.o eightball.o timer.o irc.o
CFLAGS+=-std=c99 -Wall -pedantic -O2 -g -I/usr/include/postgresql -D_POSIX_C_SOURCE=200112L
LDFLAGS+=-lpcre -lpthread -lcurl -lpq -lc

ALL = cbot

cbot: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

clean:
	rm -f cbot $(OBJECTS)
