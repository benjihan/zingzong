#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong (for GNU/make)
#
# by Benjamin Gerard 
#

SHELL      = /bin/sh
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(abspath $(srcdir)/..)
VPATH      = $(srcdir)
PKGCONFIG  = pkg-config
PACKAGE   := zingzong
VERSION   := $(shell $(srcdir)/vcversion.sh || echo X)
ifeq ($(VERSION),X)
$(error vcversion.sh failed)
endif

# ----------------------------------------------------------------------

ifndef CFLAGS
CFLAGS := -Wall
ifdef DEBUG
CFLAGS += -O0 -g -DDEBUG=$(DEBUG)
else
CFLAGS += -O2
endif
endif

# ----------------------------------------------------------------------
#  libao (AO_CFLAGS="-DNO_AO" to disable)

ifeq ($(NO_AO),1)
override AO_CFLAGS=-DNO_AO=1
endif

ifndef AO_CFLAGS
AO_LIBS := $(shell $(PKGCONFIG) --libs ao || echo X)
ifeq ($(AO_LIBS),X)
$(error pkg-config failed -- ao --libs)
endif
AO_CLAGS := $(shell $(PKGCONFIG) --cflags ao || echo X)
ifeq ($(AO_CLAGS),X)
$(error pkg-config failed -- ao --cflags)
endif
AO_VERSION := $(shell $(PKGCONFIG) --modversion ao || echo n/a)
endif

# ----------------------------------------------------------------------
#  samplerate (SRATE_CFLAGS="-DNO_SRATE" to disable)

# ifeq ($(NO_SRATE),1)
# override SRATE_CFLAGS=-DNO_SRATE=1
# endif

# ifndef SRATE_CFLAGS
# SRATE_LIBS := $(shell $(PKGCONFIG) --libs samplerate || echo X)
# ifeq ($(SRATE_LIBS),X)
# $(error pkg-config failed -- samplerate --libs)
# endif
# SRATE_CLAGS := $(shell $(PKGCONFIG) --cflags samplerate || echo X)
# ifeq ($(SRATE_CLAGS),X)
# $(error pkg-config failed -- samplerate --cflags)
# endif
# endif

# ----------------------------------------------------------------------
#  soxr (SOXR_CFLAGS="-DNO_SOXR" to disable)

ifeq ($(NO_SOXR),1)
override SOXR_CFLAGS=-DNO_SOXR=1
endif

ifndef SOXR_CFLAGS
SOXR_LIBS := $(shell $(PKGCONFIG) --libs soxr || echo X)
ifeq ($(SOXR_LIBS),X)
$(error pkg-config failed -- soxr --libs)
endif
SOXR_CLAGS := $(shell $(PKGCONFIG) --cflags soxr || echo X)
ifeq ($(SOXR_CLAGS),X)
$(error pkg-config failed -- soxr --cflags)
endif
endif

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

#override CPPFLAGS += -I$(srcdir)
override CPPFLAGS += -I.

original_CPPFLAGS := $(CPPFLAGS)
original_CFLAGS   := $(CFLAGS)
original_LDLIBS   := $(LDLIBS)


zingzong_CPPFLAGS  = $(original_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zingzong_CFLAGS    = $(original_CFLAGS) $(AO_CFLAGS) $(SOXR_CFLAGS) 
zingzong_LDLIBS    = $(original_LDLIBS) $(AO_LIBS) $(SOXR_LIBS)

# ----------------------------------------------------------------------
#  Build 
# ----------------------------------------------------------------------

all: zingzong
.PHONY: all

clean:
	-rm -f -- zingzong $(objects) $(depends)

.PHONY: clean

zingzong: NAME     = zingzong
zingzong: CPPFLAGS = $(zingzong_CPPFLAGS)
zingzong: CFLAGS   = $(zingzong_CFLAGS)
zingzong: LDLIBS   = $(zingzong_LDLIBS)


mix := mix_none mix_qerp mix_soxr
out := out_ao out_raw
vfs := vfs_file
zz  := zz_bin zz_load zz_msg zz_play zz_str zz_vfs

sources := $(addsuffix .c, $(zz) $(vfs) $(mix) $(out))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)

zingzong: $(objects)

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

depend:
.PHONY: depend

%.d: %.c ; $(CC) $(CPPFLAGS) $(CFLAGS) -MM -MMD -c $<

ifneq ($(MAKECMDGOALS),clean)
include $(depends)
endif

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
