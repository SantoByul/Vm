CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: vm

vm: vm.c
	$(CC) $(CFLAGS) -o vm vm.c

clean:
	rm -f vm correct.txt