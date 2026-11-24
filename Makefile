

CC = gcc

CFLAGS = -Wall -Wextra -std=c17

all: simulador

simulador: simulador.c

	$(CC) $(CFLAGS) simulador.c -o simulador


clean:
	rm -f simulador
