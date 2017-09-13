#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong m68k player (for GNU/make)
#
# by Benjamin Gerard AKA Ben/OVR
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
#
# GB: gcc m68k target options of interest.
#
# -mpcrel
#   Use the pc-relative addressing mode of the 68000 directly, instead
#   of using a global offset table.  At present, this option implies
#   -fpic, allowing at most a 16-bit offset for pc-relative addressing.
#   -fPIC is not presently supported with -mpcrel, though this could be
#   supported for 68020 and higher processors.
#
# -mshort (inv. -mno-short)
#   Consider type "int" to be 16 bits wide, like "short int".
#   Additionally, parameters passed on the stack are also aligned to a
#   16-bit boundary even on targets whose API mandates promotion to
#   32-bit.
#
# -malign-int (inv. -mno-align-int)
#   Control whether GCC aligns "int", "long", "long long", "float",
#   "double", and "long double" variables on a 32-bit boundary
#   (-malign-int) or a 16-bit boundary (-mno-align-int).  Aligning
#   variables on 32-bit boundaries produces code that runs somewhat
#   faster on processors with 32-bit busses at the expense of more
#   memory.
#
#
#

CC          = m68k-sc68-elf-gcc
AS          = $(CC:gcc=as)
OBJCOPY     = $(CC:gcc=objcopy)
m68_CFLAGS  = -mcpu=68000 -mtune=68000 -mshort -mno-align-int -mpcrel
gcc_CFLAGS  = -Wall -fshort-enums -fno-common
#rel_CFLAGS  = -g -Ofast -fomit-frame-pointer
rel_CFLAGS  = -g -Os -fomit-frame-pointer
dbg_CFLAGS  = -g -O0
NOSTARTFILE = -nostartfiles
NODEFLIB    = -nodefaultlibs
NOSTDLIB    = -nostdlib
VASM        = vasmm68k_mot -Fbin -quiet -no-opt -o
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

ifdef DEBUG
zz_CPPFLAGS = $(dbg_CFLAGS) -DDEBUG=$(DEBUG) -I$(srcdir)
else
zz_CPPFLAGS = $(rel_CFLAGS) -DNDEBUG=1 -I$(srcdir)
endif

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

# GB: Bummer ! We can not simply alias mem???() function by there
#     zz_mem???() counterpart as the latter might be call by the
#     former into an infinite recursive loop.
gcc_LDFLAGS = #-Wl,--defsym,memset=zz_memset,--defsym,memcpy=zz_memcpy


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

# %.elf: override CPPFLAGS := $(zz_CPPFLAGS)
# %.elf: override CFLAGS   := $(zz_CFLAGS)
%.elf: override LDFLAGS  := $(zz_LDFLAGS)
%.o: override CPPFLAGS := $(zz_CPPFLAGS)
%.o: override CFLAGS   := $(zz_CFLAGS)

zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zz_play.o: override CFLAGS = $(zz_CFLAGS) -fno-jump-tables


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
