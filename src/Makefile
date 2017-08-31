#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong (for GNU/make)
#
# by Benjamin Gerard AKA Ben/OVR
#

PACKAGE   := zingzong
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir)/..)
VERSION   := $(shell $(topdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
PKGCONFIG  = pkg-config
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)
STRIP      = strip

# ----------------------------------------------------------------------

exename = $1
libname = $1.a
dllname = $1.dll

# ----------------------------------------------------------------------

targets = $(zingzong_exe) # $(zingzong_dll)

zingzong_exe = $(call exename,zingzong)
zingzong_lib = $(call libname,zingzong)
zingzong_dll = $(call dllname,zingzong)

# ----------------------------------------------------------------------

vpath %.c $(srcdir)

# ----------------------------------------------------------------------
#  Setup some default CFLAGS
#
ifndef CFLAGS
CFLAGS := -Wall -fvisibility=hidden
ifdef DEBUG
CFLAGS += -Og -g -DDEBUG=$(DEBUG)
else
CFLAGS += -Ofast -DNDEBUG=1
endif
ifdef PROFILE
CFLAGS += -pg
endif
endif

ifeq (0,$(words $(filter -I.,-I%,\
	$(CPPFLAGS) $(gb_CPPFLAGS) $(CFLAGS) $(gb_CFLAGS))))
override CPPFLAGS += -I.
endif

# Save original CFLAGS for later use
override org_CPPFLAGS := $(CPPFLAGS)
override org_CFLAGS   := $(CFLAGS)
override org_LDLIBS   := $(LDLIBS)
override org_LDFLAGS  := $(LDFLAGS)

# ----------------------------------------------------------------------
#  libao (NO_AO=1 to disable)
# ----------------------------------------------------------------------

ifeq ($(NO_AO),1)
override AO_CFLAGS=-DNO_AO=1
override AO_LIBS=
else
ifeq ($(origin AO_CFLAGS)/$(origin AO_LIBS),undefined/undefined)

AO_CFLAGS := $(call pkgconfig,--cflags ao)
ifeq ($(AO_CFLAGS),n/a)
$(error pkg-config failed -- ao --cflags)
endif

AO_LIBS := $(call pkgconfig,--libs ao)
ifeq ($(AO_LIBS),n/a)
$(error pkg-config failed -- ao --libs)
endif

endif
endif

override gb_CFLAGS += $(AO_CFLAGS)
override gb_LDLIBS += $(AO_LIBS)

# ----------------------------------------------------------------------
#  samplerate (NO_SRATE=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SRATE),0)
override SRATE_CFLAGS=
override SRATE_LIBS=
else
ifeq ($(origin SRATE_CFLAGS)/$(origin SRATE_LIBS),undefined/undefined)

SRATE_CFLAGS := $(call pkgconfig,--cflags samplerate)
ifeq ($(SRATE_CFLAGS),n/a)
$(error pkg-config failed -- samplerate --cflags)
endif
SRATE_CFLAGS += -DWITH_SRATE=1

SRATE_LIBS := $(call pkgconfig,--libs samplerate)
ifeq ($(SRATE_LIBS),n/a)
$(error pkg-config failed -- samplerate --libs)
endif

endif
endif

override gb_CFLAGS += $(SRATE_CFLAGS)
override gb_LDLIBS += $(SRATE_LIBS)

# ----------------------------------------------------------------------
#  soxr (NO_SOXR=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SOXR),0)
override SOXR_CFLAGS=
override SOXR_LIBS=
else
ifeq ($(origin SOXR_CFLAGS)/$(origin SOXR_LIBS),undefined/undefined)

SOXR_CFLAGS := $(call pkgconfig,--cflags soxr)
ifeq ($(SOXR_CFLAGS),n/a)
$(error pkg-config failed -- soxr --cflags)
endif
SOXR_CFLAGS += -DWITH_SOXR=1

SOXR_LIBS := $(call pkgconfig,--libs soxr)
ifeq ($(SOXR_LIBS),n/a)
$(error pkg-config failed -- soxr --libs)
endif

endif
endif

override gb_CFLAGS += $(SOXR_CFLAGS)
override gb_LDLIBS += $(SOXR_LIBS)

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

zz_CPPFLAGS = $(org_CPPFLAGS) $(gb_CPPFLAGS)
zz_CFLAGS   = $(org_CFLAGS) $(gb_CFLAGS)
zz_LDLIBS   = $(org_LDLIBS) $(gb_LDLIBS)
zz_LDFLAGS  = $(org_LDFLAGS) $(gb_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: $(targets)
.PHONY: all

$(zingzong_exe) $(zingzong_dll): override CPPFLAGS = $(zz_CPPFLAGS)
$(zingzong_exe) $(zingzong_dll): override CFLAGS   = $(zz_CFLAGS)
$(zingzong_exe): override LDLIBS  = $(zz_LDLIBS)
$(zingzong_exe): override LDFLAGS = $(zz_LDFLAGS)

zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

zingzong_dll_src = $(zingzong_vfs_src)
zingzong_exe_src = $(zingzong_out_src) $(zingzong_vfs_src)
zingzong_lib_src = $(zingzong_mix_src) $(zingzong_zzz_src)

zingzong_dll_obj = $(zingzong_dll_src:.c=.o)
zingzong_exe_obj = $(zingzong_exe_src:.c=.o)
zingzong_lib_obj = $(zingzong_lib_src:.c=.o)

zingzong_mix_src = $(addsuffix .c,$(mix))
zingzong_out_src = $(addsuffix .c,$(out))
zingzong_vfs_src = $(addsuffix .c,$(vfs))
zingzong_zzz_src = $(addsuffix .c,$(zzz))

zingzong_mix_obj = $(zingzong_mix_src:.c=.o)
zingzong_out_obj = $(zingzong_out_src:.c=.o)
zingzong_vfs_obj = $(zingzong_vfs_src:.c=.o)
zingzong_zzz_obj = $(zingzong_zzz_src:.c=.o)

mix := mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
out := out_ao out_raw
vfs := vfs_file
zzz := zz_load zz_init zz_play zz_bin zz_str zz_vfs zz_mixers zz_log

sources = $(sort $(zingzong_exe_src) $(zingzong_lib_src) $(zingzong_dll_src))
headers = zingzong.h zz_private.h zz_def.h mix_common.c
objects = $(sources:.c=.o)

$(zingzong_dll): $(zingzong_lib_obj) $(zingzong_dll_obj)
$(zingzong_exe): $(zingzong_lib_obj) $(zingzong_exe_obj)
$(zingzong_lib): $(zingzong_lib_obj:%=$(zingzong_lib)(%))

%.dll:
	$(CC) -mdll $(LDFLAGS) $(LDLIBS) -o $@ $^

clean_files += $(zingzong_lib) $(zingzong_exe) $(zingzong_dll)

# ----------------------------------------------------------------------
#  Clean
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

include $(topdir)/src/make.depend

# ----------------------------------------------------------------------
#  Distrib
# ----------------------------------------------------------------------

dist_top_lst = LICENSE README.md vcversion.sh
dist_src_lst = README.md Makefile $(sources) $(headers) make.clean	\
make.depend make.dist
dist_amp_lst = README.md Makefile in_zingzong.c in_zingzong.h
dist_68k_lst = README.md Makefile mix_aga.c player.c startup.s test.s	\
test_sc68.s zz_m68k.c

dist_lst = \
 $(addprefix ./,$(dist_top_lst)) \
 $(addprefix src/,$(dist_src_lst)) \
 $(addprefix src/zz-winamp/,$(dist_amp_lst)) \
 $(addprefix src/zz-m68k/,$(dist_68k_lst))

include $(topdir)/src/make.dist

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

ifndef prefix
PREFIX = $(error 'prefix' must be set to install)
else
PREFIX = $(prefix)
endif

bindir  = $(call PREFIX)/bin
INSTALL = install $(INSTALL_OPT)

INSTALL_EXEC   = $(INSTALL) -t "$(DESTDIR)$(bindir)" "$(1)"
INSTALL_DATA   =
UNINSTALL_EXEC = \
[ ! -e "$(DESTDIR)$(bindir)/$(notdir $(1))" ] \
|| rm -- "$(DESTDIR)$(bindir)/$(notdir $(1))"
UNINSTALL_DATA =

install-strip: INSTALL_OPT = --strip-program=$(STRIP) -s
install-strip: install
install: install-exec install-data 

install-exec: $(target)
	@$(call INSTALL_EXEC,$^)
	@echo "$^ installed in $(DESTDIR)$(bindir)"

install-data:
	@$(call INSTALL_DATA)

.PHONY: install-strip install install-exec install-data

uninstall-exec:
	@$(call UNINSTALL_EXEC,$(target))
	@echo "zingzong removed from $(DESTDIR)$(bindir)"

uninstall-data:
	@$(call UNINSTALL_DATA)

uninstall: uninstall-exec uninstall-data

.PHONY: uninstall uninstall-exec uninstall-data

# ----------------------------------------------------------------------
#  Just in case redefining default rules is needed
# ----------------------------------------------------------------------

ifdef MAKERULES
include $(MAKERULES)
endif
