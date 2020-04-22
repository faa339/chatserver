all: server client 

server: server.c headers.h
	gcc -o server server.c -std=c99 -Wall -O1 -pthread

client: client.c headers.h
	gcc -o client client.c -std=c99 -Wall -O1

clean:
	rm -f client server