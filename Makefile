ifdef VSCMD_VER
	CC := cl
	CFLAGS := /nologo /W3
	OUTCMD := /Fe
	OBJEXT := obj
	EXEEXT := .exe
	LDFLAGS := /link /SUBSYSTEM:CONSOLE
	RM := del
else
	CC := cc
	CFLAGS := -Og -g3 -std=c99 -pedantic -Wall -Wextra
	OUTCMD := -o
	OBJEXT := o
	EXEEXT :=
	LDLIBS += -lm
endif

CFLAGS += $(CFLAGS_EXTRA)

SRC := sftdemo.c schrift.c
OBJ := $(SRC:.c=.$(OBJEXT))

all: sftdemo$(EXEEXT)
sftdemo$(EXEEXT): $(OBJ)
	$(CC) $(CFLAGS) $(OUTCMD)$@ $^ $(LDFLAGS) $(LDLIBS)

%.obj: %.c
	$(CC) $(CFLAGS) /Fo$@ /c /TC $^

clean:
	$(RM) sftdemo$(EXEEXT) *.$(OBJEXT)
