# Build options (override on the command line, e.g. make DUMP_PERIOD_S=5):
#   DUMP_PERIOD_S - seconds between periodic dumps (default 1, minimum 1)
#   DELAY_MS      - "slow" target only: ms each worker waits after a match
#
# Examples:
#   make                                   normal build, dump every second
#   make slow                              test build, DELAY_MS per match
#   make slow DELAY_MS=200 DUMP_PERIOD_S=10
#                                          matches build up for 10 s, so the
#                                          interactive "dump" has something to show
# Run "make clean" when switching options; every target writes ./file-finder.

CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -pthread
DUMP_PERIOD_S ?= 1
DELAY_MS ?= 1000

.PHONY: slow clean

file-finder: main.c file_finder.c file_finder.h
	$(CC) $(CFLAGS) -DFILE_FINDER_DUMP_PERIOD_S=$(DUMP_PERIOD_S) -o file-finder main.c file_finder.c

# Manual-testing build: waits DELAY_MS ms after each match.
slow: main.c file_finder.c file_finder.h
	$(CC) $(CFLAGS) -DFILE_FINDER_DUMP_PERIOD_S=$(DUMP_PERIOD_S) -DFILE_FINDER_TEST_DELAY=$(DELAY_MS) -o file-finder main.c file_finder.c

clean:
	rm -f file-finder
