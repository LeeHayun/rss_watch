CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS += -Wall -pedantic -Wno-parentheses
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)

all: rss_watch

rss_watch: $(OBJS)
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -rf rss_watch $(OBJS)

.PHONY: all clean
