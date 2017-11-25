all:
	gcc server.c -lpthread -D_REENTRANT -Wall -o server
	gcc config.c -Wall -o config
