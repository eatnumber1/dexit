.PHONY: all clean test

all: dexit

clean:
	rm -f dexit

test: dexit
	./tests.sh

%: %.c
	$(CC) -Wall -Werror -Wextra -o $@ $<
