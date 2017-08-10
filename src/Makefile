#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# Makefile for zinzong (for GNU/make)
#
# by Benjamin Gerard AKA Ben/OVR
#

SHELL      = /bin/sh
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir)/..)
PKGCONFIG  = pkg-config
PACKAGE   := zingzong
VERSION   := $(shell $(srcdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)
STRIP      = strip

# ----------------------------------------------------------------------

target     = zingzong

# ----------------------------------------------------------------------

vpath %.c $(srcdir)

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
 -DPACKAGE_NAME='"$(NAME)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'

override CPPFLAGS += -I.

org_CPPFLAGS := $(CPPFLAGS)
org_CFLAGS   := $(CFLAGS)
org_LDLIBS   := $(LDLIBS)
org_LDFLAGS  := $(LDFLAGS)

zz_CPPFLAGS   = $(org_CPPFLAGS) $(gb_CPPFLAGS)
zz_CFLAGS     = $(org_CFLAGS) $(gb_CFLAGS)
zz_LDLIBS     = $(org_LDLIBS) $(gb_LDLIBS)
zz_LDFLAGS    = $(org_LDFLAGS) $(gb_LDFLAGS)

# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: $(target)
.PHONY: all

$(target): override NAME     = zingzong
$(target): override CPPFLAGS = $(zz_CPPFLAGS) $(PACKAGE_CPPFLAGS)
$(target): override CFLAGS   = $(zz_CFLAGS)
$(target): override LDLIBS   = $(zz_LDLIBS)
$(target): override LDFLAGS  = $(zz_LDFLAGS)

mix := mix_void mix_none mix_lerp mix_qerp mix_soxr mix_srate mix_float
out := out_ao out_raw
vfs := vfs_file
zz  := zz_load zz_init zz_play zz_bin zz_str zz_vfs zz_mixers zz_msg

sources := $(addsuffix .c,$(zz) $(vfs) $(mix) $(out))
objects := $(sources:.c=.o)
depends := $(sources:.c=.d)
headers := zingzong.h zz_private.h mix_common.c

$(target): $(objects)

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
	@[ ! -e $(dist_base) ] || { echo $(dist_base) already exist; false; }
	@[ ! -e _build-$(dist_base) ] || rm -rf -- _build-$(dist_base)
	@tar xf $(dist_arch)
	@chmod -R ug-w $(dist_base)/
	@echo "extracted -- \"$(dist_arch)\""

dist-all: all distrib

.IGNORE: dist-make 	 # detect error on presence of _build-distbase
dist-make: dist-extract
	@mkdir -- _build-$(dist_base) && \
	echo "compiling -- $(dist_base)" && \
	{ test "x$(MAKERULES)" = x || \
	cp -- "$(MAKERULES)" _build-$(dist_base)/; } && \
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
