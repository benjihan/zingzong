#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong m68k player (for GNU/make)
#
# by Benjamin Gerard
#

targets    = zingzong-aga.bin zingzong-st.bin

PACKAGE   := zingzong-m68k
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir)/../..)
VERSION   := $(shell $(topdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
PKGCONFIG  = pkg-config
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

# ----------------------------------------------------------------------

vpath %.c $(srcdir) $(srcdir)/..

# ----------------------------------------------------------------------

CC          = m68k-sc68-elf-gcc
AS          = $(CC:gcc=as)
OBJCOPY     = $(CC:gcc=objcopy)
m68_CFLAGS  = -m68000 -mpcrel -mno-short -mno-align-int
GCC_CFLAGS  = -Wall -fshort-enums -fno-common -fPIC
REL_CFLAGS  = -g -Os -fomit-frame-pointer
DBG_CFLAGS  = -g -O0
NOSTARTFILE = -nostartfiles
NODEFLIB    = -nodefaultlibs
NOSTDLIB    = -nostdlib
VASM        = vasmm68k_mot -Fbin -quiet -no-opt -o
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

ifdef DEBUG
CFLAGS += $(DBG_CFLAGS) -DDEBUG=$(DEBUG)
else
CFLAGS += $(REL_CFLAGS) -DNDEBUG=1
endif

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

m68_WL = -Wl,-EB,--gc-sections,--no-undefined,--no-dynamic-linker,--unresolved-symbols=report-all
#,--verbose=5

m68_LDFLAGS = -e _start $(NOSTARTFILE) $(NOSTDLIB) $(NODEFLIB) $(m68_WL)

zz_CPPFLAGS = -DZZ_MINIMAL $(gb_CPPFLAGS)
zz_CFLAGS   = $(m68_CFLAGS) $(gb_CFLAGS)
zz_LDFLAGS  = $(m68_LDFLAGS) $(gb_LDFLAGS)

common =  player zz_m68k zz_play zz_init
amiga  = mix_aga
atari  = mix_st

atari_src = $(addsuffix .c,$(common) $(atari))
amiga_src = $(addsuffix .c,$(common) $(amiga))

atari_obj = startup.o $(atari_src:%.c=%.o)
amiga_obj = startup.o player.o $(amiga_src:%.c=%.o)

sources := $(sort $(atari_src) $(amiga_src))
objects := $(sort $(atari_obj) $(amiga_obj))
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

# ----------------------------------------------------------------------
#  Targets
# ----------------------------------------------------------------------

all: $(targets)
.PHONY: all

zingzong-aga.elf: $(amiga_obj)
zingzong-st.elf: $(atari_obj)

%.elf: override CPPFLAGS = $(zz_CPPFLAGS)
%.elf: override CFLAGS   = $(zz_CFLAGS)
%.elf: override LDFLAGS  = $(m68_LDFLAGS) $(gb_LDFLAGS)
zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

%.elf:; $(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^


# ----------------------------------------------------------------------
#  Test sc68 files
# ----------------------------------------------------------------------

clean_files += test.sc68 test.dat $(targets:%.bin=%.elf)

test: test.sc68
.PHONY: test

test.sc68: test.dat
test.dat: $(targets) test.4v test.set

.INTERMEDIATE: test.dat

%.sc68: %_sc68.s	;$(VASM) $@ $<
%.dat: %.s 		;$(VASM) $@ $<
%.bin: %.elf		;$(ELF2BIN) $< $@


#-Wl,-e,0x10000 -Wl,-Ttext,0x10000


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
