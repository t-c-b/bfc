CC = gcc
CFLAGS = -std=c11 -Wall
OBJS = main.o compiler.o
all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS)
clean:
	rm *.out *.o
