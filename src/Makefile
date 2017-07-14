#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong (for GNU/make)
#
# by Benjamin Gerard
#

SHELL	   = /bin/sh
srcdir	  := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir	  := $(realpath $(srcdir)/..)
VPATH	   = $(srcdir)
PKGCONFIG  = pkg-config
PACKAGE	  := zingzong
VERSION	  := $(shell $(srcdir)/vcversion.sh || echo X)
ifeq ($(VERSION),X)
$(error vcversion.sh failed)
endif

# ----------------------------------------------------------------------

ifndef CFLAGS
CFLAGS := -Wall # -std=c99
ifdef DEBUG
CFLAGS += -Og -g -DDEBUG=$(DEBUG)
else
CFLAGS += -Ofast -DNDEBUG=1
endif
ifdef PROFILE
CFLAGS += -pg
endif
endif

# ----------------------------------------------------------------------
#  libao (NO_AO=1 to disable)
# ----------------------------------------------------------------------

ifeq ($(NO_AO),1)
override AO_CFLAGS=-DNO_AO=1
override AO_LIBS=
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
#  samplerate (NO_SRATE=1 to disable)
# ----------------------------------------------------------------------

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
#  soxr (NO_SOXR=1 to disable)
# ----------------------------------------------------------------------

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

org_CPPFLAGS := $(CPPFLAGS)
org_CFLAGS   := $(CFLAGS)
org_LDLIBS   := $(LDLIBS)

zz_CPPFLAGS  = $(org_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zz_CFLAGS    = $(org_CFLAGS) $(AO_CFLAGS) $(SOXR_CFLAGS)
zz_LDLIBS    = $(org_LDLIBS) $(AO_LIBS) $(SOXR_LIBS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: zingzong
.PHONY: all

clean:
	-rm -f -- zingzong $(objects) $(depends)

.PHONY: clean

zingzong: override NAME	    = zingzong
zingzong: override CPPFLAGS = $(zz_CPPFLAGS)
zingzong: override CFLAGS   = $(zz_CFLAGS)
zingzong: override LDLIBS   = $(zz_LDLIBS)

mix := mix_none mix_qerp mix_soxr
out := out_ao out_raw
vfs := vfs_file
zz  := zz_bin zz_load zz_msg zz_play zz_str zz_vfs zz_mixers

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

zingzong: $(objects)

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

depend:
.PHONY: depend

%.d: %.c ; $(CC) $(zz_CPPFLAGS) $(zz_CFLAGS) -MM -MMD -c $<

ifeq ($(words $(MAKECMDGOALS)),0)
filtered = any word will do
else
filtered = $(filter-out clean% dist%,$(MAKECMDGOALS))
endif

ifneq ($(words $(filtered)),0)
include $(depends)
endif

# ----------------------------------------------------------------------
#  Distrib
# ----------------------------------------------------------------------

dist_base := zingzong-$(VERSION)
dist_arch := $(dist_base).tar.xz
dist_list := \
 $(addprefix src/,$(sources) $(headers) Makefile vcversion.sh) \
 LICENSE README.md \
 _build/build.sh

dist: distrib
distcheck: dist-check

distrib: dist-arch

dist-base:
	@test ! -d "$(dist_base)" || \
	chmod -R -- u+w $(dist_base)/ && \
	rm -rf -- $(dist_base)/
	@mkdir -- "$(dist_base)" # Fail if exist or whatever

.PHONY: 

dist-arch: dist-base
	@tar -C $(topdir) -cpf - $(dist_list) | tar -C $(dist_base) -xpf -
	@echo $(VERSION) >$(dist_base)/VERSION
	@echo $(VERSION) >$(dist_base)/src/VERSION
	@tar --owner=0 --group=0 -czpf $(dist_arch) $(dist_base)/
	@rm -rf -- $(dist_base)
	@echo "distrib file -- \"$(dist_arch)\" -- is ready"

dist-extract: dist-arch
	@test ! -e $(dist_base) || { echo $(dist_base) already exist; false; }
	@test ! -e _build-$(dist_base) || rm -rf -- _build-$(dist_base)
	@tar xf $(dist_arch)
	@chmod -R ug-w $(dist_base)/
	@echo "extracted -- \"$(dist_arch)\""

dist-all: all distrib

.IGNORE: dist-make	 # detect error on presence of _build-distbase
dist-make: dist-extract
	@mkdir -- _build-$(dist_base) && \
	echo "compiling -- $(dist_base)" && \
	$(MAKE) -sC _build-$(dist_base) \
		-f ../$(dist_base)/src/Makefile dist-all && \
	rm -rf -- _build-$(dist_base)

dist-sweep:
	@test ! -d $(dist_base) || \
	chmod -R -- u+w $(dist_base) && rm -rf -- $(dist_base)
	@test ! -d _build-$(dist_base) || {\
		rm -rf -- _build-$(dist_base); \
		echo "compilation of $(dist_base) has failed"; \
		false; }

dist-check: dist-make dist-sweep

.PHONY: distcheck dist-check distrib dist
.PHONY: dist-make dist-extract dist-arch dist-base

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

INSTALL_EXEC   = $(error install not supported yet)
INSTALL_DATA   = $(error install not supported yet)
UNINSTALL_EXEC = $(error uninstall not supported yet)
UNINSTALL_DATA = $(error uninstall not supported yet)

install: install-exec install-data

install-exec: zingzong
	$(call INSTALL_EXEC $^)

uninstall-exec: zingzong
	$(call UNINSTALL_EXEC $^)

install-data:
	$(call INSTALL_DATA $^)

.PHONY: install install-exec install-data


uninstall: uninstall-exec uninstall-data
	$(call INSTALL_EXEC $^)

uninstall-exec uninstall-data:

.PHONY: uninstall uninstall-exec uninstall-data
