CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
RAYLIB  ?= $(HOME)/raylib/src
LDLIBS   = $(RAYLIB)/libraylib.a -lm -lpthread -ldl -lX11

.PHONY: all run clean

all: class

class: class.c
	$(CC) $(CFLAGS) -I$(RAYLIB) -o $@ $< $(LDLIBS)

run: class
	./run.sh

clean:
	rm -f class class.log
