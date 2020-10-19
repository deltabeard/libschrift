NAME		:= libschrift
DESCRIPTION	:= A lightweight TrueType font rendering library.
COMPANY		:= libschrift
COPYRIGHT	:= Copyright (c) 2019, 2020 Thomas Oltmann
LICENSE_SPDX	:= ISC

# Default compiler options for GCC and Clang
CC	:= cc
OBJEXT	:= o
RM	:= rm -f
EXEOUT	:= -o
CFLAGS	:= -std=c99 -pedantic -Wall -Wextra -O2 -g3
EXE	:= $(NAME)
LICENSE := $(COPYRIGHT); Released under the $(LICENSE_SPDX) License.
GIT_VER := $(shell git describe --dirty --always --tags --long)

define help_txt
$(DESCRIPTION)
$(NAME) comes with no build time options.

$(LICENSE)
endef

SRCS := sch2bmp.c schrift.c
OBJS := $(SRCS:.c=.$(OBJEXT))

# File extension ".exe" is automatically appended on MinGW and MSVC builds, even
# if we don't ask for it.
ifeq ($(OS),Windows_NT)
	EXE := $(NAME).exe
endif

ifeq ($(GIT_VER),)
	GIT_VER := LOCAL
endif

all: $(NAME)
$(NAME): $(OBJS) $(RES)
	$(CC) $(CFLAGS) $(EXEOUT)$@ $^ $(LDFLAGS)

%.obj: %.c
	$(CC) $(CFLAGS) /Fo$@ /c /TC $^

%.res: %.rc
	rc /nologo /DCOMPANY="$(COMPANY)" /DDESCRIPTION="$(DESCRIPTION)" \
		/DLICENSE="$(LICENSE)" /DGIT_VER="$(GIT_VER)" \
		/DNAME="$(NAME)" $^

clean:
	$(RM) *.$(OBJEXT) $(EXE) $(RES)

help:
	@cd
	$(info $(help_txt))
