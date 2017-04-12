OBJECTS=main.o config.o web.o log.o eightball.o timer.o irc.o entities.o markov.o
CFLAGS+=-std=c99 -Wall -Wextra -pedantic -pie -fPIE -fstack-check -fstack-protector-all -O2 -g -I/usr/include/postgresql -D_POSIX_C_SOURCE=200809L -D_FORTIFY_SOURCE=2
LDFLAGS+=-lpcre -lpthread -lcurl -lpq -lc -L/lib/x86_64-linux-gnu -z relro

ALL = cbot

cbot: $(OBJECTS)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f cbot $(OBJECTS)
