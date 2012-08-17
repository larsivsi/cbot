OBJECTS=main.o config.o title.o log.o eightball.o timer.o
CFLAGS+=-std=c99 -Wall -pedantic -O2 -g -I/usr/include/postgresql
LDFLAGS+=-lpcre -lpthread -lcurl -lpq -lc

ALL = cbot

cbot: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

clean:
	rm -f cbot $(OBJECTS)
