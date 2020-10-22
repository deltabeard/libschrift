ifdef VSCMD_VER
	CC := cl
	CFLAGS := /W3
	OUTCMD := /Fe
else
	CC := cc
	CFLAGS := -Og -g3 -std=c99 -pedantic -Wall -Wextra
	OUTCMD := -o
endif

SDL2_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL2_LDLIBS := $(shell pkg-config --libs sdl2)

CFLAGS += -I.. $(SDL2_CFLAGS) $(CFLAGS_EXTRA)
LDLIBS += -lm $(SDL2_LDLIBS)

all: sftdemo
sftdemo: sftdemo.o schrift.o
	$(CC) $(CFLAGS) $(OUTCMD)$@ $^ $(LDLIBS)
