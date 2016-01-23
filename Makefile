CC = gcc
PORT=59098
CFLAGS= -DPORT=\$(PORT) -g -Wall
CFLAGS = -Wall -g

poll_server: poll_server.o lists.o lists.h 
	$(CC) $(CFLAGS) -o poll_server poll_server.o lists.o 

poll_server.o: poll_server.c lists.h lists.c
	$(CC) $(CFLAGS) -c poll_server.c

lists.o: lists.c lists.h
	$(CC) $(CFLAGS) -c lists.c

clean: 
	rm *.o poll_server