
CC=gcc
CFLAGS= -O0 -g
INCLUDE=-I/usr/local -I./
SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=.o)
LIBS=-lcurl

all: multiproxy 

clean:
	rm $(OBJECTS) multiproxy

multiproxy: $(OBJECTS)
	$(CC) -o multiproxy $(OBJECTS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

