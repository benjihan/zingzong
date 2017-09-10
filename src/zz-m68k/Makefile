#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong m68k player (for GNU/make)
#
# by Benjamin Gerard
#

targets    = zingzong.bin

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
gcc_CFLAGS  = -Wall -fshort-enums -fno-common  # -fPIC
rel_CFLAGS  = -g -Ofast -fomit-frame-pointer
dbg_CFLAGS  = -g -O0
NOSTARTFILE = -nostartfiles
NODEFLIB    = -nodefaultlibs
NOSTDLIB    = -nostdlib
VASM        = vasmm68k_mot -Fbin -quiet -no-opt -o
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

ifdef DEBUG
zz_CPPFLAGS = $(dbg_CFLAGS) -DDEBUG=$(DEBUG)  -I$(srcdir)
else
zz_CPPFLAGS = $(REL_CFLAGS) -DNDEBUG=1 -I$(srcdir)
endif

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

m68_WL = -Wl,-EB,--gc-sections,--no-undefined,--no-dynamic-linker,--unresolved-symbols=report-all
#,--verbose=5

m68_LDFLAGS = -e _start $(NOSTARTFILE) $(NOSTDLIB) $(NODEFLIB) $(m68_WL)

zz_CPPFLAGS += -DSC68 -DZZ_MINIMAL $(gb_CPPFLAGS) 
zz_CFLAGS   += $(gcc_CFLAGS) $(m68_CFLAGS) $(gb_CFLAGS)
zz_LDFLAGS  += $(gcc_LDFLAGS) $(m68_LDFLAGS) $(gb_LDFLAGS)

com = player
zzz = zz_m68k zz_play zz_init zz_str zz_mem zz_log
mix = mix_aga mix_st mix_stf

sources := $(sort $(addsuffix .c,$(com) $(zzz) $(mix)))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)

# ----------------------------------------------------------------------
#  Targets
# ----------------------------------------------------------------------

all: $(targets)
.PHONY: all

zingzong.elf: startup.o $(objects)

%.elf: override CPPFLAGS := $(zz_CPPFLAGS)
%.elf: override CFLAGS   := $(zz_CFLAGS)
%.elf: override LDFLAGS  := $(m68_LDFLAGS) $(gb_LDFLAGS)
zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

%.elf:; $(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^


# ----------------------------------------------------------------------
#  Test sc68 files
# ----------------------------------------------------------------------

clean_files += test.sc68 test.dat $(targets:%.bin=%.elf)

test: test.sc68
.PHONY: test

test.sc68: test.dat
test.dat: $(targets)

.INTERMEDIATE: test.dat

%.sc68: %_sc68.s %.dat
	$(VASM) $@ $<


%_sc68.dat: %.s		;$(VASM) $@ $<
%.dat: %.s %.4v %.set	;$(VASM) $@ $<
%.bin: %.elf		;$(ELF2BIN) $< $@

#test_sc68.S
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
