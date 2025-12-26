.PHONY: all clean test

CFLAGS := -Wall -Werror -Wextra

all: dexit

clean:
	rm -f dexit

test: dexit
	./tests.sh

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
