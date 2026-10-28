CC = gcc
PKG_CONFIG ?= pkg-config
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g
CPPFLAGS += -Isrc
SDL_CFLAGS = $(shell $(PKG_CONFIG) --cflags sdl3)
# Drop -mwindows so SDL errors stay visible in the console.
SDL_LIBS = $(filter-out -mwindows,$(shell $(PKG_CONFIG) --libs sdl3))

.PHONY: all run test clean

all: chip8.exe

chip8.exe: src/main.c src/chip8.c src/display.c src/chip8.h src/display.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) src/main.c src/chip8.c src/display.c $(LDFLAGS) $(SDL_LIBS) -o $@

tests/test_chip8.exe: tests/test_chip8.c src/chip8.c src/chip8.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_chip8.c src/chip8.c $(LDFLAGS) -o $@

test: tests/test_chip8.exe
	./tests/test_chip8.exe

run: chip8.exe
	./chip8.exe "$(ROM)"

clean:
	rm -f chip8.exe tests/test_chip8.exe
