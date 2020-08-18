CC=gcc
CFLAGS = -g
OBJS=coroutine.o  server.o
TARGET=server client

all : $(TARGET)

server:$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

client:
	$(CC) $(CFLAGS) client.c -o client

.PHONY: clean
clean :
	rm -rf *.o $(TARGET)

