CC = gcc
CFLAGS = -Wall -Werror -g -lm


all:
	$(CC) $(CFLAGS) -o vm vm.c
