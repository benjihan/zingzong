#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong (for GNU/make)
#
# by Benjamin Gerard 
#

SHELL      = /bin/sh
srcdir    := $(dir $(lastword $(MAKEFILE_LIST)))
topdir    := $(abspath $(srcdir)..)
VPATH      = $(srcdir)
PKGCONFIG  = pkg-config
PACKAGE   := zingzong
VERSION   := $(shell $(srcdir)vcversion.sh || echo X)
ifeq ($(VERSION),X)
$(error vcversion.sh failed)
endif

# ----------------------------------------------------------------------

ifndef CFLAGS
CFLAGS := -Wall -O2
endif

# ----------------------------------------------------------------------
#  libao (AO_CFLAGS="-DNO_AO" to disable)

ifndef AO_CFLAGS
AO_LIBS := $(shell $(PKGCONFIG) --libs ao || echo X)
ifeq ($(AO_LIBS),X)
$(error pkg-config failed -- ao --libs)
endif
AO_CLAGS := $(shell $(PKGCONFIG) --cflags ao || echo X)
ifeq ($(AO_CLAGS),X)
$(error pkg-config failed -- ao --cflags)
endif
endif

# ----------------------------------------------------------------------
#  samplerate (SRATE_CFLAGS="-DNO_SRATE" to disable)

ifndef SRATE_CFLAGS
SRATE_LIBS := $(shell $(PKGCONFIG) --libs samplerate || echo X)
ifeq ($(SRATE_LIBS),X)
$(error pkg-config failed -- samplerate --libs)
endif
SRATE_CLAGS := $(shell $(PKGCONFIG) --cflags samplerate || echo X)
ifeq ($(SRATE_CLAGS),X)
$(error pkg-config failed -- samplerate --cflags)
endif
endif

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

original_CPPFLAGS := $(CPPFLAGS)
original_CFLAGS   := $(CFLAGS)
original_LDLIBS   := $(LDLIBS)

zingzong_CPPFLAGS  = $(original_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zingzong_CFLAGS    = $(original_CFLAGS) $(AO_CFLAGS) $(SRATE_CFLAGS)
zingzong_LDLIBS    = $(original_LDLIBS) $(AO_LIBS) $(SRATE_LIBS)

# ----------------------------------------------------------------------
#  Build 
# ----------------------------------------------------------------------

all: zingzong
.PHONY: all

clean: ; -rm -f zingzong zingzafe
.PHONY: clean

zingzong: NAME     = zingzong
zingzong: CPPFLAGS = $(zingzong_CPPFLAGS)
zingzong: CFLAGS   = $(zingzong_CFLAGS)
zingzong: LDLIBS   = $(zingzong_LDLIBS)

# ----------------------------------------------------------------------
#  Distrib
# ----------------------------------------------------------------------

dist_base := zingzong-$(VERSION)
dist_arch := $(dist_base).tar.xz
dist_list := \
 LICENSE README.md \
 src/zingzong.c src/Makefile src/vcversion.sh \
 _build/build.sh

dist: distrib
distcheck: dist-check
distrib: dist-arch

dist-arch: $(dist_base)
	@tar -C $(topdir) -cpf - $(dist_list) | tar -C $(dist_base) -xpf -
	@echo $(VERSION) >$(dist_base)/VERSION
	@echo $(VERSION) >$(dist_base)/src/VERSION
	@tar --owner=0 --group=0 -czpf $(dist_arch) $(dist_base)/
	@rm -rf -- $(dist_base)
	@echo "distrib file -- \"$(dist_arch)\" -- is ready"

dist-check: dist
	@test ! -e $(dist_base) || { echo $(dist_base) already exist; false; }
	@test ! -e _build-$(dist_base) || rm -rf -- _build-$(dist_base)
	@tar xf $(dist_arch)
	@chmod -R ug-w $(dist_base)/
	@mkdir _build-$(dist_base)
	@$(MAKE) -C _build-$(dist_base) -f ../$(dist_base)/src/Makefile all dist
	@rm -rf -- _build-$(dist_base)/ $(dist_base)/


$(dist_base):
	@test ! -e "$@" || rm -r -- "$@" && mkdir -- "$@"

.PHONY: distcheck dist-check dist distrib dist-arch
