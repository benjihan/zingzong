#! /usr/bin/make -f 
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong m68k player (for GNU/make)
#
# by Benjamin Gerard AKA Ben/OVR
#

targets := zingzong.bin
PACKAGE := zingzong-m68k
srcdir  := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir  := $(realpath $(srcdir)/../..)
VERSION := $(shell $(topdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
PKGCONFIG := pkg-config
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

# ----------------------------------------------------------------------

vpath %.c $(srcdir) $(srcdir)/..

# ----------------------------------------------------------------------
#  Tools
# ----------------------------------------------------------------------

CC      = m68k-sc68-elf-gcc	# GB: any m68k-elf gcc should do
AS      = $(CC:gcc=as)
LD      = $(CC:gcc=ld)
OBJCOPY = $(CC:gcc=objcopy)
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

# ----------------------------------------------------------------------
#
# m68_CFLAGS: Compile flags for all m68k targets.
# m68_CPU: CPU specific flags (default to 68000)
# m68_ALIGN: How to align variable (default is no-align)
#
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

m68_CPU      = 68000
m68_ALIGN    = no-align-int
m68_SHORT    = short

m68_CFLAGS  += -mcpu=$(m68_CPU) -mtune=$(m68_CPU)
m68_CFLAGS  += -m$(m68_SHORT) -m$(m68_ALIGN)
m68_CFLAGS  += -m$(m68_pcrel)
gcc_CFLAGS  += -Wall -fshort-enums -fno-common -fno-jump-tables

zz_CPPFLAGS += -I$(srcdir) -DSC68 -DZZ_MINIMAL
ifdef DEBUG
m68_pcrel    = no-pcrel
zz_CFLAGS   += -g -Og
zz_CPPFLAGS += -DDEBUG=1
else
m68_pcrel    = pcrel
zz_CFLAGS   += -g -Os -fomit-frame-pointer
zz_CPPFLAGS += -DNDEBUG=1 -I$(srcdir)
endif

zz_ASFLAGS  += --register-prefix-optional # no annoying %
zz_ASFLAGS  += --base-size-default-16     # (a0,d0) is (a0,d0.w)
zz_ASFLAGS  += --disp-size-default-16     # default word size

null  :=
space := ${null} ${null}
comma := ,

comma_list = $(subst ${space},${comma},$(strip $1))

filter-m = $(patsubst -m%,%,$(filter -m%,$(1)))

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

com = player
zzz = zz_m68k zz_play zz_init zz_str zz_mem zz_log
mix = mix_aga mix_stf mix_ste mix_fal

sources := $(sort $(addsuffix .c,$(com) $(zzz) $(mix)))
depends := $(sources:.c=.d)

# ----------------------------------------------------------------------
#  Targets
# ----------------------------------------------------------------------

TARGET_MACH = -mcpu=$(m68_CPU) -march=$(m68_CPU)
TARGET_ARCH = $(TARGET_MACH) -mtune=$(m68_CPU)

CPPFLAGS = $(zz_CPPFLAGS)
CFLAGS   = $(zz_CFLAGS) $(gcc_CFLAGS) $(m68_CFLAGS)
ASFLAGS  = $(call comma_list,-Wa $(zz_ASFLAGS))

.SUFFIXES:

all: $(targets)
.PHONY: all

zz_play.o: override CPPFLAGS += $(PACKAGE_CPPFLAGS)

START_SYMB = $(error START_SYMB must be defined)
START_ADDR = $(error START_ADDR must be defined)
ELFFLAGS   = $(error ELFFLAGS must be defined)

ELFSTATIC  = -static
ELFPARTIAL = -e $(START_SYMB) -Ttext=0x$(START_ADDR)
ELFCOMMON  = -EB $(ELFSTATIC) $(ELFPARTIAL)

# ----------------------------------------------------------------------
#  Drivers
# ----------------------------------------------------------------------

drivers := $(patsubst %,dri_%.elf,aga stf ste fal)

drivers: $(drivers)
.PHONY: drivers

dri_aga.elf: mix_aga.o
dri_stf.elf: mix_stf.o m68k_stf.o
dri_ste.elf: mix_ste.o
dri_fal.elf: mix_fal.o

# GB: Drivers have unresolved symbols that are set to 0 by the
#     linker. Setting a START_ADDRESS far away triggers an overflow
#     error with 16-bit relocations (R_68K_PC16).
DRIFLAGS = $(ELFCOMMON) --unresolved-symbols=ignore-all

dri_%.elf: START_SYMB = mixer_$*
dri_%.elf: START_ADDR = 00000000
dri_%.elf: ELFPARTIAL = -r
dri_%.elf: ELFFLAGS   = $(DRIFLAGS)

# ----------------------------------------------------------------------
#  Player
# ----------------------------------------------------------------------

player: zingzong.bin
.PHONY: player

zingzong.elf: START_SYMB = player_start
zingzong.elf: START_ADDR = C0000000
zingzong.elf: ELFFLAGS   = $(ELFCOMMON) -dT zingzong.x -Map $*.map
zingzong.elf: player_start.o $(addsuffix .o,$(com) $(zzz)) $(drivers)
.PRECIOUS: zingzong.elf
player_start.o: zingzong.x

# ----------------------------------------------------------------------
#  Test sc68 files
# ----------------------------------------------------------------------

CLEAN_TARGETS += clean-test

clean-test: ; @rm -f -- *.o *.dri *.elf *.map; echo "$@ done"
.PHONY: clean-test

test: test.sc68
.PHONY: test

test.elf: START_SYMB = sc68_start
test.elf: START_ADDR = 00010000
test.elf: ELFFLAGS   = $(ELFCOMMON) -dT sc68.x -Map $*.map
test.elf: sc68_start.o $(addsuffix .o,$(com) $(zzz)) $(drivers) song.elf vset.elf

sc68_start.o: sc68.x		# GB: hacky

SONGFILE = Cowboy_Theme-01.4v
ifndef VSETFILE
VSETFILE = $(SONGFILE:%.4v=%.set)
endif

vset.elf song.elf: %.elf: %.o %.dat
	@echo "[OBJ] $*";\
	$(OBJCOPY) --add-section .$*.dat=$*.dat $< $@

song.dat: $(SONGFILE)
vset.dat: $(VSETFILE)
song.dat vset.dat: %.dat:
	test -n "$<" &&\
	echo "[DAT] $* [$<]" &&\
	cat $< >$@

VASMBIN = vasmm68k_mot -quiet -Fbin -o 

%.sc68: %_sc68.s test.bin
	@echo "[ASM] $@"; $(VASMBIN) $@ $<

.INTERMEDIATE: test.bin

# ----------------------------------------------------------------------
#  Implicite rules
# ----------------------------------------------------------------------


%.o : %.c
	@echo "[GCC] $* <$(call filter-m,$(CFLAGS))>";\
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o "$@" "$<"

%.o : %.S
	@echo "[GAS] $* <$(call filter-m,$(CFLAGS))>";\
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ASFLAGS) -c -o "$@" "$<"

%.elf dri_%.elf:
	@echo "[ELF] $* [$(START_SYMB)=$(START_ADDR)]";\
	$(LD) $(ELFFLAGS) -o $@ $^

%.bin : %.elf
	@echo "[BIN] $*";\
	$(ELF2BIN) $< $@

# ----------------------------------------------------------------------
#  Includes
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean
include $(topdir)/src/make.depend
ifdef MAKERULES
include $(MAKERULES)
endif
