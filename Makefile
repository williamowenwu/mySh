CC=gcc
CFLAGS= -g -Wall -Werror -std=c11

all: mysh

shell: mysh.c
	$(CC) $(CFLAGS) -o mysh mysh.c

clean:
	rm -f mysh
