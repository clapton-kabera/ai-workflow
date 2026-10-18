CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -g
# SPEC NOTE: GNU ld --wrap lets TC-40 fail the next malloc without changing dict.h.
LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc

.PHONY: all test memcheck clean

all: test_dict

test_dict: dict.c test_dict.c dict.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dict.c test_dict.c

test: test_dict
	./test_dict

memcheck: test_dict
	valgrind --leak-check=full --error-exitcode=2 ./test_dict

clean:
	rm -f test_dict
