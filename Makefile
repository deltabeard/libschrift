ifdef VSCMD_VER
	CC := cl
	RM := del
	OUTCMD := /Fe
	OBJEXT := obj
	CFLAGS := /nologo /W3 /GL
	LDLIBS := /link /SUBSYSTEM:CONSOLE
	SDL2_CFLAGS := /Iext\inc
	SDL2_LDLIBS := /LIBPATH:ext\lib SDL2main.lib SDL2-static.lib \
		winmm.lib msimg32.lib version.lib imm32.lib setupapi.lib \
		kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
		advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
		odbc32.lib odbccp32.lib
else
	CC := cc
	CFLAGS := -Og -g3 -std=c99 -pedantic -Wall -Wextra
	OUTCMD := -o
	OBJEXT := o
	LDLIBS := -lm
endif

SDL2_CFLAGS ?= $(shell pkg-config --cflags sdl2)
SDL2_LDLIBS ?= $(shell pkg-config --libs sdl2)

CFLAGS += $(SDL2_CFLAGS) $(CFLAGS_EXTRA)
LDLIBS += $(SDL2_LDLIBS)

SRCS += sftdemo.c schrift.c
OBJS += $(SRCS:.c=.$(OBJEXT))

all: sftdemo
sftdemo: $(OBJS)
	$(CC) $(CFLAGS) $(OUTCMD)$@ $^ $(LDLIBS)

%.obj: %.c
	$(CC) $(CFLAGS) /Fo$@ /c /TC $^
	
clean:
	$(RM) *.obj *.o *.exe sftdemo
