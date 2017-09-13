#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# GNU/Makefile for zinzong - library and cli
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

# ----------------------------------------------------------------------

vpath %.c $(srcdir)

# ----------------------------------------------------------------------

exename = $1
libname = $1.a
dllname = $1.dll

# ----------------------------------------------------------------------

targets = $(zz_exe) # $(zz_dll)

zz_exe = $(call exename,zingzong)
zz_lib = $(call libname,zingzong)
zz_dll = $(call dllname,zingzong)

# ----------------------------------------------------------------------
#  Toolchain
# ----------------------------------------------------------------------

ifndef STRIP
ifeq ($(CC:%gcc=gcc),gcc)
STRIP = $(CC:%gcc=%strip)
else
STRIP = strip
endif
endif

# ----------------------------------------------------------------------
#  Setup some default CFLAGS
# ----------------------------------------------------------------------

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

l_patterns := -l% -Wl,-l%
L_patterns := -L%
filter-L = $(strip $(filter     $(L_patterns),$1))
filter-l = $(strip $(filter-out $(L_patterns),$1))

# ----------------------------------------------------------------------
#  libao (NO_AO=1 to disable)
# ----------------------------------------------------------------------

ifeq ($(NO_AO),1)
override undefine AO_CFLAGS
override undefine AO_LIBS
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

ifeq ($(AO_LIBS),)
override gb_CPPFLAGS += -DNO_AO=1
endif

override gb_CFLAGS  += $(AO_CFLAGS)
override gb_LDFLAGS += $(call filter-L,$(AO_LIBS))
override gb_LDLIBS  += $(call filter-l,$(AO_LIBS))

# ----------------------------------------------------------------------
#  samplerate (NO_SRATE=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SRATE),0)
override undefine SRATE_CFLAGS
override undefine SRATE_LIBS
else
ifeq ($(origin SRATE_CFLAGS)/$(origin SRATE_LIBS),undefined/undefined)

SRATE_CFLAGS := $(call pkgconfig,--cflags samplerate)
ifeq ($(SRATE_CFLAGS),n/a)
$(error pkg-config failed -- samplerate --cflags)
endif

SRATE_LIBS := $(call pkgconfig,--libs samplerate)
ifeq ($(SRATE_LIBS),n/a)
$(error pkg-config failed -- samplerate --libs)
endif

endif
endif

ifneq ($(SRATE_LIBS),)
override gb_CPPFLAGS += -DWITH_SRATE=1
endif
override gb_CFLAGS  += $(SRATE_CFLAGS)
override gb_LDFLAGS += $(call filter-L,$(SRATE_LIBS))
override gb_LDLIBS  += $(call filter-l,$(SRATE_LIBS))


# ----------------------------------------------------------------------
#  soxr (NO_SOXR=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SOXR),0)
override undefine SOXR_CFLAGS
override undefine SOXR_LIBS
else
ifeq ($(origin SOXR_CFLAGS)/$(origin SOXR_LIBS),undefined/undefined)

SOXR_CFLAGS := $(call pkgconfig,--cflags soxr)
ifeq ($(SOXR_CFLAGS),n/a)
$(error pkg-config failed -- soxr --cflags)
endif

SOXR_LIBS := $(call pkgconfig,--libs soxr)
ifeq ($(SOXR_LIBS),n/a)
$(error pkg-config failed -- soxr --libs)
endif

endif
endif

ifneq ($(SOXR_LIBS),)
override gb_CPPFLAGS += -DWITH_SOXR=1
endif
override gb_CFLAGS  += $(SOXR_CFLAGS)
override gb_LDFLAGS += $(call filter-L,$(SOXR_LIBS))
override gb_LDLIBS  += $(call filter-l,$(SOXR_LIBS))

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

$(zz_exe) $(zz_dll): override CPPFLAGS = $(zz_CPPFLAGS)
$(zz_exe) $(zz_dll): override CFLAGS   = $(zz_CFLAGS)
$(zz_exe): override LDLIBS  = $(zz_LDLIBS)
$(zz_exe): override LDFLAGS = $(zz_LDFLAGS)

zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

zz_dll_src = $(zz_vfs_src)
zz_exe_src = $(zz_out_src) $(zz_vfs_src)
zz_lib_src = $(zz_mix_src) $(zz_pla_src) $(zz_zzz_src)

zz_dll_obj = $(zz_dll_src:.c=.o)
zz_exe_obj = $(zz_exe_src:.c=.o)
zz_lib_obj = $(zz_lib_src:.c=.o)

zz_mix_src = $(addsuffix .c,$(mix))
zz_out_src = $(addsuffix .c,$(out))
zz_vfs_src = $(addsuffix .c,$(vfs))
zz_pla_src = $(addsuffix .c,$(pla))
zz_zzz_src = $(addsuffix .c,$(zzz))

zz_mix_obj = $(zz_mix_src:.c=.o)
zz_out_obj = $(zz_out_src:.c=.o)
zz_vfs_obj = $(zz_vfs_src:.c=.o)
zz_pla_obj = $(zz_pla_src:.c=.o)
zz_zzz_obj = $(zz_zzz_src:.c=.o)

mix := mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
out := out_ao out_raw
vfs := vfs_file
pla := zz_init zz_play zz_log
zzz := zz_load zz_bin zz_mem zz_str zz_vfs zz_mixers

sources = $(sort $(zz_exe_src) $(zz_lib_src) $(zz_dll_src))
headers = zingzong.h zz_private.h zz_def.h mix_common.c
objects = $(sources:.c=.o)

$(zz_dll): $(zz_lib_obj) $(zz_dll_obj)
$(zz_exe): $(zz_lib_obj) $(zz_exe_obj)
$(zz_lib): $(zz_lib_obj:%=$(zz_lib)(%))

%.dll:
	$(CC) -mdll $(LDFLAGS) $(LDLIBS) -o $@ $^

clean_files += $(zz_lib) $(zz_exe) $(zz_dll)

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
dist_68k_lst = README.md Makefile mix_aga.c mix_st.c zz_m68k.c		\
player.c sc68_debug.h startup.s test.s test_sc68.s m68k_muldiv.h

dist_lst = $(sort \
 $(addprefix ./,$(dist_top_lst)) \
 $(addprefix src/,$(dist_src_lst)) \
 $(addprefix src/zz-winamp/,$(dist_amp_lst)) \
 $(addprefix src/zz-m68k/,$(dist_68k_lst)) )

include $(topdir)/src/make.dist

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

ifndef bin_targets
bin_targets = $(targets)
endif

ifndef prefix
PREFIX = $(error 'prefix' must be set to install)
else
PREFIX = $(prefix)
endif

bindir  = $(call PREFIX)/bin
INSTALL = install $(INSTALL_OPT)

INSTALL_EXEC   = \
mkdir -p -- "$(DESTDIR)$(1)" && \
$(INSTALL) -t "$(DESTDIR)$(1)" $(foreach f,$(2),"$(f)")
UNINSTALL_EXEC = rm -f -- $(foreach f,$(2),"$(DESTDIR)$(1)/$(f)")
INSTALL_DATA   =
UNINSTALL_DATA =

install-strip: INSTALL_OPT = --strip-program=$(STRIP) -s
install-strip: install
install: install-exec install-data 
	@echo $@ done

install-exec: $(bin_targets)
	$(call INSTALL_EXEC,$(bindir),$^)
	@echo $@ done

install-data:
	$(call INSTALL_DATA)
	@echo $@ done

.PHONY: install-strip install install-exec install-data


uninstall-exec:
	$(call UNINSTALL_EXEC,$(bindir),$(bin_targets))
	@echo $@ done

uninstall-data:
	$(call UNINSTALL_DATA)
	@echo $@ done

uninstall: uninstall-exec uninstall-data
	@echo $@ done


.PHONY: uninstall uninstall-exec uninstall-data

# ----------------------------------------------------------------------
#  Just in case redefining default rules is needed
# ----------------------------------------------------------------------

ifdef MAKERULES
include $(MAKERULES)
endif
