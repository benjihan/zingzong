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
VERSION	  := $(shell $(srcdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

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
override AO_VERSION=n/a
else

ifndef AO_CFLAGS
AO_CFLAGS := $(call pkgconfig,--cflags ao)
ifeq ($(AO_CFLAGS),n/a)
$(error pkg-config failed -- ao --cflags)
endif
endif

ifndef AO_LIBS
AO_LIBS := $(call pkgconfig,--libs ao)
ifeq ($(AO_LIBS),n/a)
$(error pkg-config failed -- ao --libs)
endif
endif

AO_VERSION := $(call pkgconfig,--modversion ao)
endif

override bin_CFLAGS += $(AO_CFLAGS)
override bin_LDLIBS += $(AO_LIBS)

# ----------------------------------------------------------------------
#  samplerate (NO_SRATE=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SRATE),0)
override SRATE_CFLAGS=
override SRATE_LIBS=
override SRATE_VERSION=n/a
else

ifndef SRATE_CFLAGS
SRATE_CFLAGS := $(call pkgconfig,--cflags samplerate)
ifeq ($(SRATE_CFLAGS),n/a)
$(error pkg-config failed -- samplerate --cflags)
endif
SRATE_CFLAGS += -DWITH_SRATE=1
endif

ifndef SRATE_LIBS
SRATE_LIBS := $(call pkgconfig,--libs samplerate)
ifeq ($(SRATE_LIBS),n/a)
$(error pkg-config failed -- samplerate --libs)
endif
endif

SRATE_VERSION := $(call pkgconfig,--modversion samplerate)
endif

override bin_CFLAGS += $(SRATE_CFLAGS)
override bin_LDLIBS += $(SRATE_LIBS)

# ----------------------------------------------------------------------
#  soxr (NO_SOXR=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SOXR),0)
override SOXR_CFLAGS=
override SOXR_LIBS=
override SOXR_VERSION=n/a
else

ifndef SOXR_CFLAGS
SOXR_CFLAGS := $(call pkgconfig,--cflags soxr)
ifeq ($(SOXR_CFLAGS),n/a)
$(error pkg-config failed -- soxr --cflags)
endif
endif

ifndef SOXR_LIBS
SOXR_LIBS := $(call pkgconfig,--libs soxr)
ifeq ($(SOXR_LIBS),n/a)
$(error pkg-config failed -- soxr --libs)
endif
SOXR_CFLAGS += -DWITH_SOXR=1
endif

SOXR_VERSION := $(call pkgconfig,--modversion samplerate)
endif

override bin_CFLAGS += $(SOXR_CFLAGS)
override bin_LDLIBS += $(SOXR_LIBS)

# ----------------------------------------------------------------------
#  smarc (NO_SMARC=0 to enable)
# ----------------------------------------------------------------------

ifneq ($(NO_SMARC),0)
override SMARC_CFLAGS=
override SMARC_LIBS=
override SMARC_VERSION=n/a
else

ifndef SMARC_LIBS
SMARC_LIBS := -lsmarc
SOXR_CFLAGS += -DWITH_SMARC=1
endif
SMARC_VERSION := n/a
endif

override bin_CFLAGS += $(SMARC_CFLAGS)
override bin_LDLIBS += $(SMARC_LIBS)

# ----------------------------------------------------------------------

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

override CPPFLAGS += -I.

org_CPPFLAGS := $(CPPFLAGS)
org_CFLAGS   := $(CFLAGS)
org_LDLIBS   := $(LDLIBS)
org_LDFLAGS  := $(LDFLAGS)

zz_CPPFLAGS   = $(org_CPPFLAGS) $(bin_CPPFLAGS)
zz_CFLAGS     = $(org_CFLAGS) $(bin_CFLAGS)
zz_LDLIBS     = $(org_LDLIBS) $(bin_LDLIBS)
zz_LDFLAGS    = $(org_LDFLAGS) $(bin_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: zingzong
.PHONY: all

zingzong: override NAME	    = zingzong
zingzong: override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)
zingzong: override CFLAGS   = $(zz_CFLAGS)
zingzong: override LDLIBS   = $(zz_LDLIBS)
zingzong: override LDFLAGS  = $(zz_LDFLAGS)

mix := mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_smarc mix_float
out := out_ao out_raw
vfs := vfs_file
zz  := zz_bin zz_load zz_msg zz_play zz_str zz_vfs zz_mixers

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

zingzong: $(objects)

# ----------------------------------------------------------------------
#  Clean
# ----------------------------------------------------------------------

clean-dep:
	@-rm -f -- $(depends)

clean-obj:
	@-rm -f -- $(objects)

clean: clean-dep clean-obj
	@-rm -f -- zingzong
	@echo "zingzong has been cleaned up"

.PHONY: clean clean-obj clean-dep

# ----------------------------------------------------------------------
#  Dependencies
# ----------------------------------------------------------------------

.PHONY: depend

%.d: %.c ;$(CC) $(zz_CPPFLAGS) $(zz_CFLAGS) -MM -MMD -c $<

ifeq ($(words $(MAKECMDGOALS)),0)
filtered = ST forever
else
filtered = $(filter-out clean% dist%,$(MAKECMDGOALS))
endif

ifneq ($(words $(filtered)),0)
-include $(depends)
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

.IGNORE: dist-make 	 # detect error on presence of _build-distbase
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
