OBJECTS=main.o config.o web.o log.o eightball.o timer.o irc.o utf8.o crypto.o entities.o markov.o
CFLAGS+=-std=c99 -Wall -Wextra -pedantic -O2 -g -I/usr/include/postgresql -D_POSIX_C_SOURCE=200112L
LDFLAGS+=-lpcre -lpthread -lcurl -lpq -lc -L/lib/x86_64-linux-gnu

ALL = cbot

cbot: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

clean:
	rm -f cbot $(OBJECTS)
