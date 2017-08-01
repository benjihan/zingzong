#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong winamp plugin (for GNU/make)
#
# by Benjamin Gerard
#

SHELL      = /bin/sh
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir)/../..)
PKGCONFIG  = pkg-config
PACKAGE   := zingzong-winamp
VERSION   := $(shell $(srcdir)/../vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

# ----------------------------------------------------------------------

vpath %.c $(srcdir) $(srcdir)/..

# ----------------------------------------------------------------------

target     = in_zingzong.dll

# ----------------------------------------------------------------------

CC      = i686-w64-mingw32-gcc
STRIP	= $(CC:-gcc=-strip)

override NO_AO = 1

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
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

override CPPFLAGS += -I.

org_CPPFLAGS := $(CPPFLAGS)
org_CFLAGS   := $(CFLAGS)
org_LDLIBS   := $(LDLIBS)
org_LDFLAGS  := $(LDFLAGS)

zz_CPPFLAGS   = $(org_CPPFLAGS) $(gb_CPPFLAGS) -DNO_AO=1
zz_CFLAGS     = $(org_CFLAGS) $(gb_CFLAGS)
zz_LDLIBS     = $(org_LDLIBS) $(gb_LDLIBS)
zz_LDFLAGS    = $(org_LDFLAGS) $(gb_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: $(target)
.PHONY: all

$(target): override NAME	   = zingzong (winamp)
$(target): override CPPFLAGS = $(zz_CPPFLAGS)
$(target): override CFLAGS   = $(dll_CFLAGS) $(zz_CFLAGS)
$(target): override LDLIBS   = $(zz_LDLIBS)
$(target): override LDFLAGS  = $(zz_LDFLAGS)

in_zingzong.o: override CPPFLAGS = $(WAINC) $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)

mix := mix_void mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
out := 
vfs := vfs_file
zz  := zz_bin zz_load zz_msg zz_play zz_str zz_vfs zz_mixers
src := in_zingzong

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out) $(src))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

zingzong: $(objects)

# ----------------------------------------------------------------------
#  Link plugin dll
# ----------------------------------------------------------------------

$(target): $(objects)
	$(CC) -mdll -o $@ $^ -static-libgcc -static $(LDFLAGS) $(LDLIBS)

# ----------------------------------------------------------------------
#  Clean
# ----------------------------------------------------------------------

clean-dep:
	@-rm -f -- $(depends)

clean-obj:
	@-rm -f -- $(objects)

clean: clean-dep clean-obj
	@-rm -f -- $(target)
	@echo "$(target) has been cleaned up"

.PHONY: clean clean-obj clean-dep

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

.PHONY: depend

in_zingzong.d: override zz_CPPFLAGS := $(zz_CPPFLAGS) $(WAINC)

%.d: %.c ;$(CC) $(zz_CPPFLAGS) $(zz_CFLAGS) -MM -MMD -c $<

ifeq ($(words $(MAKECMDGOALS)),0)
filtered = ST forever
else
filtered = $(filter-out clean% dist%,$(MAKECMDGOALS))
endif

ifeq ($(NODEPS),1)
filtered =
endif

ifneq ($(words $(filtered)),0)
-include $(depends)
endif


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
install: $(target)
	@$(call INSTALL_PLUG,$^)
	@echo $^ installed in "$(DESTDIR)$(WADLL)"

uninstall:
	@$(call UNINSTALL_PLUG,$(target))
	@echo zingzong removed from "$(DESTDIR)$(WADLL)"

.PHONY: install-strip install uninstall

# ----------------------------------------------------------------------
# Just in case redefining default rules is needed
# ----------------------------------------------------------------------

ifdef MAKERULES
include $(MAKERULES)
endif
