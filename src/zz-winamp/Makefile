#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# GNU/Makefile for zinzong - winamp plugin
#
# by Benjamin Gerard AKA Ben/OVR
#

PACKAGE   := zingzong
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

targets    = in_zingzong.dll

# ----------------------------------------------------------------------
#   Toolchain
# ----------------------------------------------------------------------

ifeq ($(origin CC),default)
CC = i686-w64-mingw32-gcc
endif

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
CFLAGS = -m32 -mwin32 -mdll -Wall
ifdef DEBUG
CFLAGS  += -Og -g
else
CFLAGS  += -Ofast
endif
ifdef PROFILE
CFLAGS  += -pg
endif
endif # ifndef CFLAGS

ifndef CPPFLAGS
ifdef DEBUG
CPPFLAGS  = -DDEBUG=$(DEBUG)
else
CPPFLAGS  = -DNDEBUG
endif
CPPFLAGS += -I. $(WAINC)
endif # ifndef CPPFLAGS

ifndef LDFLAGS
LDFLAGS = -static-libgcc -static
endif # ifndef LDFLAGS

# ----------------------------------------------------------------------
#  Winamp SDK
# ----------------------------------------------------------------------

# Winamp SDK
ifndef WASDK
WASDK = $(error WASDK is undefined)
else
ifeq ($(words $(filter %-cygwin,$(MAKE_HOST))),1)
WASDK := $(shell cygpath -ua "$(WASDK)")
endif
endif
WAINC = -I$(WASDK) -I$(WASDK)/wasabi

# Windows Platform SDK
ifndef WPSDK
WPSDK = $(error WPSDK is undefined)
endif

# ----------------------------------------------------------------------
#  LIBS/LDFLAGS filter
# ----------------------------------------------------------------------

l_patterns := -l% -Wl,-l%
filter-L    = $(strip $(filter-out $(l_patterns),$1))
filter-l    = $(strip $(filter     $(l_patterns),$1))

# ----------------------------------------------------------------------
#  No libao
# ----------------------------------------------------------------------

override gb_CPPFLAGS += -DNO_AO=1

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
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

zz_CPPFLAGS = $(error zz_CPPFLAGS)
zz_CFLAGS   = $(error zz_CFLAGS)
zz_LDLIBS   = $(error zz_LDLIBS)
zz_LDFLAGS  = $(error zz_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

.SUFFIXES:

all: $(targets)
.PHONY: all

mix := mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
zz  := zz_load zz_init zz_play zz_bin zz_str zz_vfs zz_mixers zz_log zz_mem
src := in_zingzong vfs_file

sources := $(addsuffix .c,$(src) $(zz) $(mix))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)

override CPPFLAGS += $(gb_CPPFLAGS)
override CFLAGS   += $(gb_CFLAGS)
override LDFLAGS  += $(gb_LDFLAGS)
override LDLIBS   += $(gb_LDLIBS)

in_zingzong.o zz_play.o: CPPFLAGS += $(PACKAGE_CPPFLAGS)
in_zingzong.dll: $(objects)
	@echo "[DLL] $@";\
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	@echo "[GCC] $* [$(patsubst -m%,%,$(filter -m%,$(CFLAGS)))]";\
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

# Winamp install dir
ifndef WADIR
WADIR = $(error WADIR is undefined)
endif
WADLL = $(WADIR)/Plugins
INSTALL        = install $(INSTALL_OPT)
INSTALL_PLUG   = $(INSTALL) -t "$(DESTDIR)$(WADLL)" "$(1)"
UNINSTALL_PLUG = \
[ ! -e "$(DESTDIR)$(WADLL)/$(notdir $(1))" ] \
|| rm -- "$(DESTDIR)$(WADLL)/$(notdir $(1))"

install-strip: INSTALL_OPT = --strip-program=$(STRIP) -s
install-strip: install
install: $(targets)
	@$(call INSTALL_PLUG,$^)
	@echo $^ installed in "$(DESTDIR)$(WADLL)"

uninstall:
	@$(call UNINSTALL_PLUG,$(targets))
	@echo zingzong removed from "$(DESTDIR)$(WADLL)"

.PHONY: install-strip install uninstall

# ----------------------------------------------------------------------
#  Includes
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean
include $(topdir)/src/make.depend
ifdef MAKERULES
include $(MAKERULES)
endif
