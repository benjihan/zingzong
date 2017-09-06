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
$(info $(CC:%gcc=gcc))
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
CFLAGS	:= -Wall # -std=c99
ifdef DEBUG
CFLAGS	+= -Og -g -DDEBUG=$(DEBUG)
else
CFLAGS	+= -Ofast -DNDEBUG=1
endif
ifdef PROFILE
CFLAGS	+= -pg
endif
endif
dll_CFLAGS = -m32 -mwin32 -mdll


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

l_patterns := -l% -Wl,-l%
filter-L    = $(strip $(filter-out $(l_patterns),$1))
filter-l    = $(strip $(filter     $(l_patterns),$1))

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

override gb_CFLAGS  += $(SRATE_CFLAGS)
override gb_LDFLAGS += $(call filter-L,$(SRATE_LIBS))
override gb_LDLIBS  += $(call filter-l,$(SRATE_LIBS))

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

override gb_CFLAGS  += $(SOXR_CFLAGS)
override gb_LDFLAGS += $(call filter-L,$(SOXR_LIBS))
override gb_LDLIBS  += $(call filter-l,$(SOXR_LIBS))

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

override CPPFLAGS += -I.

zz_CPPFLAGS := $(CPPFLAGS) $(gb_CPPFLAGS) -DNO_AO=1
zz_CFLAGS   := $(CFLAGS) $(gb_CFLAGS)
zz_LDLIBS   := $(LDLIBS) $(gb_LDLIBS)
zz_LDFLAGS  := $(LDFLAGS) $(gb_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: $(targets)
.PHONY: all


#in_zingzong.o: override NAME	   = zingzong (winamp)

%.o: override CPPFLAGS = $(zz_CPPFLAGS)
%.o: override CFLAGS   = $(dll_CFLAGS) $(zz_CFLAGS)
$(targets): override LDLIBS   = $(zz_LDLIBS)
$(targets): override LDFLAGS  = $(zz_LDFLAGS)

in_zingzong.o: override CPPFLAGS = $(WAINC) $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zz_play.o: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

mix := mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
out := 
vfs := vfs_file
zz  := zz_load zz_init zz_play zz_bin zz_str zz_vfs zz_mixers zz_log zz_mem
src := in_zingzong

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out) $(src))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

# ----------------------------------------------------------------------
#  Link plugin dll
# ----------------------------------------------------------------------

$(targets): $(objects)
	$(CC) -mdll -o $@ $^ -static-libgcc -static $(LDFLAGS) $(LDLIBS)

# ----------------------------------------------------------------------
#  Clean
# ----------------------------------------------------------------------

include $(topdir)/src/make.clean

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

in_zingzong.d: override zz_CPPFLAGS := $(zz_CPPFLAGS) $(WAINC)
include $(topdir)/src/make.depend

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
# Just in case redefining default rules is needed
# ----------------------------------------------------------------------

ifdef MAKERULES
include $(MAKERULES)
endif
