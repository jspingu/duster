CFLAGS += $(shell pkg-config --cflags sdl3) -ggdb -Wall -Wextra -Wpedantic -std=c23
OPTFLAGS += -Og
LDFLAGS += $(shell pkg-config --libs sdl3)

duster: Duster.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< $(LDFLAGS) -o $@

.PHONY: clean
clean:
	rm -f duster
