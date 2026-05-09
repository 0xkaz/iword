# Makefile for iWord
CC = gcc
CFLAGS = -O2 -g

# Compile all
all: iwordctl iworduse iwordserver

# Clean files
clean:
	rm -f iword.o iwordctl iwordserver

iwordctl: iword.o
	$(CC) $(CFLAGS) -o iwordctl iwordctl.c iword.o

iworduse: iword.o
	$(CC) $(CFLAGS) -o iworduse iworduse.c iword.o

iwordserver: iword.o
	$(CC) $(CFLAGS) -o iwordserver iwordserver.c iword.o -lpthread

.c.o:
	$(CC) $(CFLAGS) -c $<

iword.o:
	$(CC) $(CFLAGS) -c -o iword.o iword.c
