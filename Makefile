CFLAGS += -ggdb -Wall -Wextra -Wpedantic -std=c23
OPTFLAGS += -Og
LDFLAGS += -lSDL3

duster: Duster.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $(LDFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f duster
