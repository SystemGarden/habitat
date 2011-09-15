# Revision 1.2.  Jim Plank

CC = gcc

EXECUTABLES = main

all: $(EXECUTABLES)

main: rb.o

rb.o: rb.h
main.o: rb.h

clean:
	rm -f core *.o $(EXECUTABLES) a.out

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

main: main.o
	$(CC) $(CFLAGS) -o main main.o rb.o
