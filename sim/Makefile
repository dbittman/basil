CFLAGS=-std=gnu99 -Wall -Wextra -Wno-unused-parameter -g
all: basilsim

basilsim: sim.o
	$(CC) sim.o -o basilsim

sim.o: sim.c

clean:
	-rm *.o basilsim

