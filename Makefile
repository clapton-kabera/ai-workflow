CC     ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -g

all: test_dict

test_dict: dict.o test_dict.o
	$(CC) $(CFLAGS) -Wl,--wrap=malloc -o test_dict dict.o test_dict.o

dict.o: dict.c dict.h
	$(CC) $(CFLAGS) -c -o dict.o dict.c

test_dict.o: test_dict.c dict.h
	$(CC) $(CFLAGS) -c -o test_dict.o test_dict.c

test: test_dict
	./test_dict

memcheck: test_dict
	valgrind --leak-check=full --error-exitcode=2 ./test_dict

clean:
	rm -f dict.o test_dict.o test_dict
