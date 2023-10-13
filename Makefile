PORT=56403
LDFLAGS=-DPORT=\$(PORT) -g -std=gnu99 -Wall -Werror

all: friend_server

friend_server: *.c *.h
	gcc -o friend_server friends.c friend_server.c -I. $(LDFLAGS)

clean:
	rm -f friend_server