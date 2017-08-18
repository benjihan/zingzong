#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong m68k player (for GNU/make)
#
# by Benjamin Gerard
#

SHELL      = /bin/sh
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir)/../..)
PKGCONFIG  = pkg-config
PACKAGE   := zingzong-winamp
VERSION   := $(shell $(srcdir)/../vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

# ----------------------------------------------------------------------

vpath %.c $(srcdir) $(srcdir)/..

# ----------------------------------------------------------------------

target     = zingzong-aga.bin
targets    = $(target)

# ----------------------------------------------------------------------

CC              = m68k-sc68-elf-gcc
AS              = $(CC:gcc=as)
OBJCOPY         = $(CC:gcc=objcopy)
M68_CFLAGS      = -m68000 -mtune=68000 -mpcrel -mno-short -mno-align-int
GCC_CFLAGS      = -Wall -Wno-multichar -Wno-unused-variable
REL_CFLAGS      = -g -Os -fomit-frame-pointer
DBG_CFLAGS      = -g -O0
NOSTARTFILE     = -nostartfiles
NODEFLIB        = -nodefaultlibs
NOSTDLIB        = -nostdlib
VASM            = vasmm68k_mot -Fbin -quiet -no-opt -o
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

ifndef CFLAGS
CFLAGS	:= -Wall
ifdef DEBUG
CFLAGS	+= $(DBG_CFLAGS) -DDEBUG=$(DEBUG)
else
CFLAGS	+= $(REL_CFLAGS) -DNDEBUG=1
endif
endif

all: $(target)
.PHONY: all

zz_CPPFLAGS := $(CPPFLAGS) -DZZ_MINIMAL
zz_CFLAGS   := $(CFLAGS)
zz_LDFLAGS  := -e player_init $(NOSTARTFILE)

$(target): override NAME     = zingzong (Amiga)
$(target): override CPPFLAGS = $(zz_CPPFLAGS)
$(target): override CFLAGS   = $(M68_CFLAGS) $(zz_CFLAGS)
$(target): override LDLIBS   = $(zz_LDLIBS)
$(target): override LDFLAGS  = $(zz_LDFLAGS)

mix := mix_void mix_aga
out := 
vfs :=
zz  := zz_play zz_init
src := zz_m68k player

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out) $(src))
objects := startup.o $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

zingzong: $(objects)

# ----------------------------------------------------------------------
#  Test sc68 files
# ----------------------------------------------------------------------

clean_files += test.sc68 test.dat

test: test.sc68
.PHONY: test

test.sc68: test.dat
test.dat: $(target) test.4v test.set

.INTERMEDIATE: test.dat

%.sc68: %_sc68.s	;$(VASM) $@ $<
%.dat: %.s 		;$(VASM) $@ $<
%.bin: %.elf		;$(ELF2BIN) $< $@

$(target:bin=elf): $(objects)
	 $(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LIBS)

# ----------------------------------------------------------------------
#  Clean
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

include $(topdir)/src/make.depend

# ----------------------------------------------------------------------
# Just in case redefining default rules is needed
# ----------------------------------------------------------------------

ifdef MAKERULES
include $(MAKERULES)
endif
