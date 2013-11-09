CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11 -O3

BIN := markov

.PHONY: all clean

all: $(BIN)

clean:
	rm -rf $(BIN)

$(BIN): markov.c
	$(CC) $(CFLAGS) -o $(BIN) markov.c -lrt
