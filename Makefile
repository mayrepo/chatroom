.PHONY: clean

CFLAGS=-Wall -Wextra -std=gnu11

#may need to replace -pthread by -lpthread to compile
LDFLAGS=-pthread

all: server client

chatroom.o: chatroom.c chatroom.h common.h

common.o: common.c common.h

server.o: server.c chatroom.h common.h

server: server.o chatroom.o common.o

client.o: client.c common.h

client: client.o common.o 

clean:
	rm -rf *.o
	rm -rf server client

realclean: clean
	rm -rf *~
