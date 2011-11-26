CC=gcc

#Extra options to the compiler.
CFLAGS+=-std=c99 -Wall -pedantic -O2

#All targets listed here.
ALL = cbot

#Build all targets
all: $(ALL)

#Running "make clean" removes the files created above
clean:
	rm -f $(ALL)
