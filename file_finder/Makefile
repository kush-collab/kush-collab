CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -pthread

file-finder: main.c file_finder.c file_finder.h
	$(CC) $(CFLAGS) -o file-finder main.c file_finder.c

clean:
	rm -f file-finder
