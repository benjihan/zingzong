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
VERSION   := $(shell $(srcdir)vcversion.sh)

ifneq ($(.SHELLSTATUS),0)
$(error vcversion.sh failed with $(.SHELLSTATUS))
endif

# ----------------------------------------------------------------------

ifndef CFLAGS
CFLAGS	:= -Wall -O2
endif

ifndef AO_LIBS
AO_LIBS := $(shell $(PKGCONFIG) --libs ao)
ifneq ($(.SHELLSTATUS),0)
$(error pkg-config failed with $(.SHELLSTATUS))
endif

AO_CLAGS := $(shell $(PKGCONFIG) --cflags ao)
ifneq ($(.SHELLSTATUS),0)
$(error pkg-config failed with $(.SHELLSTATUS))
endif

endif

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

original_CPPFLAGS := $(CPPFLAGS)
original_CFLAGS   := $(CFLAGS)
original_LDLIBS   := $(LDLIBS)

zingzong_CPPFLAGS  = $(original_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zingzong_CFLAGS    = $(original_CFLAGS) $(AO_CFLAGS)
zingzong_LDLIBS    = $(original_LDLIBS) $(AO_LIBS)

zingzafe_CPPFLAGS  = $(original_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zingzafe_CFLAGS    = $(original_CFLAGS) $(AO_CFLAGS)
zingzafe_LDLIBS    = $(original_LDLIBS) $(AO_LIBS)

# ----------------------------------------------------------------------
# Build 
# ----------------------------------------------------------------------

all: zingzong #zingzafe
.PHONY: all

clean: ; -rm -f zingzong zingzafe
.PHONY: clean

zingzong: NAME     = zingzong
zingzong: CPPFLAGS = $(zingzong_CPPFLAGS)
zingzong: CFLAGS   = $(zingzong_CFLAGS)
zingzong: LDLIBS   = $(zingzong_LDLIBS)

zingzafe: NAME     = zingzafe
zingzafe: CPPFLAGS = $(zingzafe_CPPFLAGS)
zingzafe: CFLAGS   = $(zingzafe_CFLAGS)
zingzafe: LDLIBS   = $(zingzafe_LDLIBS)

# ----------------------------------------------------------------------
# Distrib
# ----------------------------------------------------------------------

dist_base := zingzong-$(VERSION)
dist_arch := $(dist_base).tar.xz
dist_list := \
 LICENSE README.md \
 src/zingzong.c src/Makefile src/vcversion.sh \
 _build/build.sh


dist: distrib
distrib: dist-arch
.PHONY: distrib

dist-arch: $(dist_base)
	@tar -C $(topdir) -cpf - $(dist_list) | tar -C $(dist_base) -xpf -
	@echo $(VERSION) >$(dist_base)/VERSION
	@echo $(VERSION) >$(dist_base)/VERSION
	@tar --owner=0 --group=0 -czpf $(dist_arch) $(dist_base)/
	@rm -rf -- $(dist_base)
	@echo "distrib file -- \"$(dist_arch)\" -- is ready"

dist-check: dist
	@test ! -e $(dist_base) || { echo $(dist_base) already exist; false; }
	@test ! -e _build-$(dist_base) || rm -rf -- _build-$(dist_base)
	@tar xf $(dist_arch)
	chmod -R ug-w $(dist_base)/
	@mkdir _build-$(dist_base)
	make -C _build-$(dist_base) -f ../$(dist_base)/src/Makefile all dist
	@rm -rf -- _build-$(dist_base)/ $(dist_base)/


$(dist_base):
	@test ! -e "$@" || rm -r -- "$@" && mkdir -- "$@"

.PHONY: dist distrib dist-arch
