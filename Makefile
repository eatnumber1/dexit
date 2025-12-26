.PHONY: all clean test print

CFLAGS := -Wall -Werror -Wextra

all: dexit

clean:
	rm -f dexit

test: dexit
	./tests.sh

# Helpful to view all the exit codes.
print: dexit
	seq 0 255 | parallel --keep-order -j100 --tag -- SHELL=bash ./dexit

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
