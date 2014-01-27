CFLAGS=-Wall -Wextra -std=gnu99 -O0
all: bas

bas: as.o
	$(CC) -o bas as.o

as.o: as.c

as.c: as.h

clean:
	rm *.o bas

