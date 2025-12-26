.PHONY: all clean test print

CFLAGS := -Wall -Werror -Wextra

all: dexit

clean:
	rm -f dexit

test: dexit
	./tests.sh

# Helpful to view all the exit codes.
print: dexit
	parallel --keep-order -j0 --tag -- SHELL=bash ./dexit ::: {0..255}

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
