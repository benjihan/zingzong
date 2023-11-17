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

.DELETE_ON_ERROR:
.ONESHELL:
ifneq ($(V),1)
.SILENT:
M = @printf "[%3s] %s\n" "$(1)" "$(2)"
else
M =
endif

# ----------------------------------------------------------------------
#  Tools
# ----------------------------------------------------------------------

# GB: any m68k elf capable  gcc should do
CC      = m68k-sc68-elf-gcc
AS      = $(CC:gcc=as)
LD      = $(CC:gcc=ld)
NM      = $(CC:gcc=nm)
OBJCOPY = $(CC:gcc=objcopy)
ELF2BIN = $(OBJCOPY) -S -Obinary --set-section-flags .bss=contents,data,load

# For test files only
VASMMOT = vasmm68k_mot -quiet $(VASMFLAGS)
VASMBIN = $(VASMMOT) -Fbin -o
VASMTOS = $(VASMMOT) -Ftos -o

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
zzo          = $(m68_o) $(zzz_o)
else
m68_pcrel    = pcrel
zz_CFLAGS   += -g -Os -fomit-frame-pointer
zz_CPPFLAGS += -DZZ_MINIMAL -DNDEBUG=1
ELFLIBS      = $(NOSTART) $(NOLIBS)
zzo          = $(m68_o) $(filter-out zz_log.o,$(zzz_o))
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

zzz = player zz_m68k zz_core zz_init zz_log mix_ata
dri = mix_aga mix_stf mix_fal $(foreach X,hub dnr lrb s7s,mix_ste_$(X))
m68 = m68k_mix m68k_mem

dri_c = $(addsuffix .c,$(dri))
dri_o = $(addsuffix .o,$(dri))
zzz_c = $(addsuffix .c,$(zzz))
zzz_o = $(addsuffix .o,$(zzz))
m68_S = $(addsuffix .S,$(m68))
m68_o = $(addsuffix .o,$(m68))

sources := $(sort $(zzz_c) $(dri_c) $(m68_S) stub.S sc68_start.S)

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

define more-help
	$(call Mline)
	$(call Minfo, test-tos,         Build TOS test programs)
	$(call Minfo, zingzong player,  Build binary player)
	$(call Minfo, drivers,          Build drivers)
	$(call Minfo, bindist,          Build distrib zip archive)
endef

define more-tools
	$(call Mline)
	$(call Mtool,VASMMOT, Devpac-like assembler)
	$(call Mtool,AS,      Assembler)
	$(call Mtool,OBJCOPY, Binary transform tool)
	$(call Mtool,NM,      List symbols)
endef


zz_core.o: override CPPFLAGS += $(PACKAGE_CPPFLAGS)
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
#      the final linker scripts.
# ----------------------------------------------------------------------

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!!                                    !!!
# !!! DO NOT forget to update zingzong.x !!!
# !!!                                    !!!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

drilist := aga stf fal dnr lrb s7s hub
drivers := $(patsubst %,dri_%.elf,$(drilist))

drivers: $(drivers)
.PHONY: drivers

dri_fal.elf: mix_fal.o
dri_aga.elf: mix_aga.o
dri_stf.elf: mix_stf.o
dri_hub.elf: mix_ste_hub.o
dri_dnr.elf: mix_ste_dnr.o
dri_s7s.elf: mix_ste_s7s.o
dri_lrb.elf: mix_ste_lrb.o

mix_ata.o: JUMP_TABLE = jump-tables


DRIFLAGS = -r -static $(NOSTART) $(NOLIBS)

# START_* are dummies only used by the echo command
dri_%.elf: START_SYMB = mixer_$*
dri_%.elf: START_ADDR = 00010000
dri_%.elf: LDFLAGS = $(DRIFLAGS)

# ----------------------------------------------------------------------
#  Binary player
# ----------------------------------------------------------------------

zingzong player: zingzong.bin zingzong.sym
.PHONY: player zingzong

ELFFLAGS = -static $(ELFLIBS) $(NOUNDEF) $(MAPFLAGS)
MAPFLAGS = $(call maybe-wl,-Map=$*.map)

zingzong_o = stub.o $(zzo) $(drivers)

.PRECIOUS: zingzong.elf
zingzong.elf: START_SYMB = zingzong
zingzong.elf: START_ADDR = C0000000
zingzong.elf: LDFLAGS    = -T $(srcdir)/zingzong.x $(ELFFLAGS)
zingzong.elf: $(zingzong_o)
stub.o: $(srcdir)/zingzong.x # GB: force rebuild when script change

# ----------------------------------------------------------------------
#  Binary distrib
# ----------------------------------------------------------------------

VW := $(subst ., ,$(VERSION))
ifeq (4,$(words $(VW)))
BINVERSION := $(subst ${space},,$(wordlist 1,3,$(VW)))_$(lastword $(VW))
else
BINVERSION := $(subst .,,$(VERSION))
endif

bindist := $(PACKAGE)-$(BINVERSION)
binfile := zingzong.{bin,sym} ${srcdir}/test_tos.s ${srcdir}/README.md

bindist: $(bindist).zip
.PHONY: bindist

$(bindist).zip: zingzong
	$(call M,ZIP,$(bindist))
	7z -bb0 -bd a $@ $(binfile) >/dev/null

# ----------------------------------------------------------------------
#  sc68 test file
# ----------------------------------------------------------------------

CLEAN_TARGETS += clean-test

CLEAN_TEST = *.o *.dri *.elf test_*.tos *.map song.dat vset.dat	\
test.sc68 test.sndh song.dep zingzong.sym

.PHONY: clean-test
clean-test:
	$(call M,CLN,$@)
	$(RM) -- $(CLEAN_TEST)

.PHONY: test
test: test.sc68 test.sndh

test_o = sc68_start.o $(zzo) $(drivers)

.PRECIOUS: test.elf
test.elf: START_SYMB = sc68_start
test.elf: START_ADDR = 000FACE00
test.elf: LDFLAGS    = -T $(srcdir)/sc68.x $(ELFFLAGS)
test.elf: $(test_o) $(drivers) song.elf vset.elf
sc68_start.o: $(srcdir)/sc68.x	# GB: force rebuild when script change

SONGFILE ?= test.4v
ifndef VSETFILE
VSETFILE = $(patsubst %$(suffix $(SONGFILE)),%.set,$(SONGFILE))
endif

# Dependencies on music files (.4v and .set)
dat_songdep = $(realpath $(SONGFILE) $(VSETFILE))
gen_songdep = echo "$(dat_songdep)"
cur_songdep = $(shell test -r song.dep && cat song.dep)
ifneq ($(cur_songdep),$(dat_songdep))
$(info Generating song.dep)
$(file >song.dep,$(dat_songdep))
endif

vset.elf song.elf: %.elf: %.o %.dat
	$(call M,OBJ,$*)
	$(OBJCOPY) --add-section .$*.dat=$*.dat $< $@

song.dat: $(SONGFILE)
vset.dat: $(VSETFILE)
song.dat vset.dat: %.dat:
	test -n "$<"
	$(call M,DAT,$* [$(notdir $(realpath $<))])
	cp -- "$<" "$@"

song.dat vset.dat: song.dep
.INTERMEDIATE: song.dat vset.dat

song_dat: song.dat vset.dat
.PHONY: song_dat

song.dep: $(SONGFILE) $(VSETFILE)
	$(call M,UPD,$@)
	$(gen_songdep) >$@

%.sc68: %_sc68.s test.bin
	$(call M,ASM,$@)
	$(VASMBIN) $@ $< -DRATE=$(or $(RATE),200)

%.sndh: %_sndh.s zingzong.bin zingzong.sym song_dat
	$(call M,ASM,$@)
	$(VASMBIN) $@ $<

# ----------------------------------------------------------------------
#  TOS test files
# ----------------------------------------------------------------------

test-tos: $(patsubst %,test_%.tos,def stf ste fal)
PHONY: test-tos

TOS_FLAGS = -DMID=$(MID) -DZZSYMB=1
ifdef SPR
TOS_FLAGS += -DSPR=$(SPR)
endif
ifdef LR8
TOS_FLAGS += -DLR8=$(LR8)
endif

MID = $(error MID must be defined)
test_def.tos: MID=0
test_stf.tos: MID=2
test_ste.tos: MID=3
test_fal.tos: MID=4
test_%.tos: test_tos.s zingzong.bin zingzong.sym song_dat
	$(call M,TOS,$@)
	$(VASMTOS) $@ $< $(TOS_FLAGS)

# ----------------------------------------------------------------------
#  Implicite rules
# ----------------------------------------------------------------------

%.o: %.c
	$(call M,GCC,$* <$(call filter-m,$(CFLAGS))>)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c  "$<" -o "$@"

%.o: %.S
	$(call M,GAS,$* <$(call filter-m,$(CFLAGS))>)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ASFLAGS) -c "$<" -o "$@"

%.elf dri_%.elf:
	$(call M,ELF,$* [$(START_SYMB)=$(START_ADDR)])
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.bin: %.elf
	$(call M,BIN,$*)
	$(ELF2BIN) --change-addresses 256 $< $@

%.sym: %.elf $(srcdir)/mksymb
	$(call M,SYM,$*)
	$(NM) -ng $< | $(srcdir)/mksymb >$@

# ----------------------------------------------------------------------
#  Includes
# ----------------------------------------------------------------------

include $(topdir)/src/make.info
include $(topdir)/src/make.clean
include $(topdir)/src/make.depend

ifdef MAKERULES
include $(MAKERULES)
endif
