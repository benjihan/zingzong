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
vpath %.S $(srcdir)
vpath %.s $(srcdir)

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
gcc_CFLAGS  += -Wall -fshort-enums -fno-common -f$(JUMP_TABLE)
JUMP_TABLE   = no-jump-tables

zz_CPPFLAGS += -I$(srcdir) -DSC68
ifdef DEBUG
m68_pcrel    = no-pcrel
m68_SHORT    = no-short
zz_CFLAGS   += -g -Og
zz_CPPFLAGS += -DNO_VFS -DDEBUG=1
ELFLIBS      = 
else
m68_pcrel    = pcrel
zz_CFLAGS   += -g -Os -fomit-frame-pointer
zz_CPPFLAGS += -DZZ_MINIMAL -DNDEBUG=1
ELFLIBS      = $(NOSTART) $(NOLIBS)
endif

zz_ASFLAGS  += --register-prefix-optional # no annoying %
zz_ASFLAGS  += --base-size-default-16     # (a0,d0) is (a0,d0.w)
zz_ASFLAGS  += --disp-size-default-16     # default word size

null  :=
space := ${null} ${null}
comma := ,

comma-list = $(subst ${space},${comma},$(strip $1))
filter-m   = $(patsubst -m%,%,$(filter -m%,$(1)))

# Depending if calling LD or CC to link
maybe-wl   = $(call comma-list,-Wl $1)

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

com = player
zzz = zz_m68k zz_play zz_init zz_str zz_mem zz_log zz_fast
mix = mix_aga mix_stf mix_ste mix_fal mix_ata m68k_mix 
m68 = m68k_mix.S m68k_mem.S

sources := $(sort $(addsuffix .c,$(com) $(zzz) $(mix)) $(m68))

# ----------------------------------------------------------------------
#  Targets
# ----------------------------------------------------------------------

TARGET_MACH = -mcpu=$(m68_CPU) -march=$(m68_CPU)
TARGET_ARCH = $(TARGET_MACH) -mtune=$(m68_CPU)

CPPFLAGS = $(zz_CPPFLAGS)
CFLAGS   = $(zz_CFLAGS) $(gcc_CFLAGS) $(m68_CFLAGS)
ASFLAGS  = $(call comma-list,-Wa $(zz_ASFLAGS))

.SUFFIXES:

all: $(targets)
.PHONY: all

zz_play.o: override CPPFLAGS += $(PACKAGE_CPPFLAGS)
zz_fast.o: JUMP_TABLE = jump-tables
zz_fast.o: override zz_CFLAGS += -Ofast

START_SYMB = $(error START_SYMB must be defined)
START_ADDR = $(error START_ADDR must be defined)

NOUNDEF     = -z defs
NOLIBS      = -nodefaultlibs -nostdlib

# ----------------------------------------------------------------------
#  Drivers
#
#  GB: Drivers have unresolved symbols that are set to 0 by the
#      linker. Setting a START_ADDRESS far away triggers an overflow
#      error with 16-bit relocations (R_68K_PC16).
#
#  GB: Drivers are basically partial (intermediate) link with a common
#      interface. All drivers could work as overlay but it's not
#      convenient for our usage. However as only one driver is used at
#      a time they can share a common BSS section. This is achieved by
#      the finale linker scripts.
#      ----------------------------------------------------------------------

drivers := $(patsubst %,dri_%.elf,aga stf ste fal) mix_ata.o $(m68:%.S=%.o)

drivers: $(drivers)
.PHONY: drivers

dri_aga.elf: mix_aga.o
dri_stf.elf: mix_stf.o
dri_ste.elf: mix_ste.o
dri_fal.elf: mix_fal.o

#mix_fal.o: m68_CPU = 68030
mix_ata.o: JUMP_TABLE = jump-tables

DRIFLAGS = -r -static $(NOSTART) $(NOLIBS)

# START_* are dummies only used by the echo command
dri_%.elf: START_SYMB = mixer_$*
dri_%.elf: START_ADDR = 00000000
dri_%.elf: LDFLAGS = $(DRIFLAGS)

# ----------------------------------------------------------------------
#  Binary player
# ----------------------------------------------------------------------

zingzong player: zingzong.bin
.PHONY: player zingzong

ELFFLAGS = -static $(ELFLIBS) $(NOUNDEF) $(MAPFLAGS)
MAPFLAGS = $(call maybe-wl,-Map=$*.map)
#MAPFLAGS = $(error MAPFLAGS ?)

.PRECIOUS: zingzong.elf
zingzong.elf: START_SYMB = zingzong
zingzong.elf: START_ADDR = C0000000
zingzong.elf: LDFLAGS    = -T $(srcdir)/zingzong.x $(ELFFLAGS)
zingzong.elf: $(addsuffix .o,stub m68k_mix $(com) $(zzz)) $(drivers)
stub.o: $(srcdir)/zingzong.x # GB: force rebuild when script change

# ----------------------------------------------------------------------
#  sc68 test file
# ----------------------------------------------------------------------

CLEAN_TARGETS += clean-test

CLEAN_TEST = *.o *.dri *.elf test_*.tos *.map song.dat vset.dat	\
test.sc68 test.sndh song.dep

.PHONY: clean-test
clean-test:
	@rm -f -- $(CLEAN_TEST) && echo "$@ done"

.PHONY: test
test: test.sc68 test.sndh

test_o = $(addsuffix .o,sc68_start $(com) $(zzz))
.PRECIOUS: test.elf
test.elf: START_SYMB = sc68_start
test.elf: START_ADDR = 00010000
test.elf: LDFLAGS    = -T $(srcdir)/sc68.x $(ELFFLAGS)
test.elf: $(test_o) $(drivers) song.elf vset.elf
sc68_start.o: $(srcdir)/sc68.x	# GB: force rebuild when script change

SONGFILE ?= test.4v
ifndef VSETFILE
VSETFILE = $(SONGFILE:%.4v=%.set)
endif

# Dependencies on .4v .set files 
song_deps := $(shell test -r song.dep && cat song.dep)
ifneq ($(song_deps),$(SONGFILE) $(VSETFILE))
$(file >song.dep,$(SONGFILE) $(VSETFILE))
$(info Generating [song.dep])
endif

vset.elf song.elf: %.elf: %.o %.dat
	echo "[OBJ] $*";\
	$(OBJCOPY) --add-section .$*.dat=$*.dat $< $@

song.dat: $(SONGFILE)
vset.dat: $(VSETFILE)
song.dat vset.dat: %.dat:
	test -n "$<" && echo "[DAT] $* [$(notdir $<)]" && cat "$<" >$@

song.dat vset.dat: song.dep

song.dep:
	@echo "[GEN] $@";\
	echo "$(SONGFILE) $(VSETFILE)">$@

VASMMOT = vasmm68k_mot -quiet $(VASMFLAGS)
VASMBIN = $(VASMMOT) -Fbin -o
VASMTOS = $(VASMMOT) -Ftos -o

%.sc68: %_sc68.s test.bin
	echo "[ASM] $@";\
	$(VASMBIN) $@ $<

%.sndh: %_sndh.s zingzong.bin
	echo "[ASM] $@";\
	$(VASMBIN) $@ $<

# ----------------------------------------------------------------------
#  TOS test files
# ----------------------------------------------------------------------

test_tos: test_def.tos test_stf.tos test_ste.tos test_fal.tos
PHONY: test_tos

test_def.tos: override VASMFLAGS:=$(VASMFLAGS) -DMIXERID=0
test_stf.tos: override VASMFLAGS:=$(VASMFLAGS) -DMIXERID=2
test_ste.tos: override VASMFLAGS:=$(VASMFLAGS) -DMIXERID=3
test_fal.tos: override VASMFLAGS:=$(VASMFLAGS) -DMIXERID=4
test_%.tos: test_tos.s test.bin
	echo "[TOS] $@";\
	$(VASMTOS) $@ $<

.INTERMEDIATE: test.bin

# ----------------------------------------------------------------------
#  Implicite rules
# ----------------------------------------------------------------------

.SILENT:

%.o : %.c
	echo "[GCC] $* <$(call filter-m,$(CFLAGS))>";\
	$(CC) $(CPPFLAGS) $(CFLAGS) -c  "$<" -o "$@"

%.o : %.S
	echo "[GAS] $* <$(call filter-m,$(CFLAGS))>";\
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ASFLAGS) -c "$<" -o "$@"

%.elf dri_%.elf:
	echo "[ELF] $* [$(START_SYMB)=$(START_ADDR)]";\
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.bin : %.elf
	echo "[BIN] $*";\
	$(ELF2BIN) $< $@

# ----------------------------------------------------------------------
#  Includes
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean
include $(topdir)/src/make.depend
ifdef MAKERULES
include $(MAKERULES)
endif
