#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# GNU/Makefile for zinzong - cli
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

.DELETE_ON_ERROR:
.ONESHELL:
ifneq ($(V),1)
M = @printf "[%3s] %s\n" "$(1)" "$(2)"
else
M =
endif

# ----------------------------------------------------------------------

exename = $1

# ----------------------------------------------------------------------

targets = $(zz_exe)
zz_exe  = $(call exename,zingzong)

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
CFLAGS = -Wall -fvisibility=hidden
ifneq ($(DEBUG),)
CFLAGS += -Og -g
else
CFLAGS += -Ofast
endif
endif # ifndef CFLAGS

ifndef CPPFLAGS
ifneq ($(DEBUG),)
CPPFLAGS = -DDEBUG=$(DEBUG)
else
CPPFLAGS = -DNDEBUG=1
endif
ifeq (0,$(words $(filter -I.,-I%,\
	$(CPPFLAGS) $(gb_CPPFLAGS) $(CFLAGS) $(gb_CFLAGS))))
override CPPFLAGS += -I.
endif
endif # ifndef CPPFLAGS

l_patterns := -l% -Wl,-l%
L_patterns := -L%
filter-L = $(strip $(filter     $(L_patterns),$1))
filter-l = $(strip $(filter-out $(L_patterns),$1))

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

PACKAGE_CPPFLAGS  = \
 -DPACKAGE_NAME='"$(PACKAGE)"' \
 -DPACKAGE_VERSION='"$(VERSION)"'


# ----------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------

all: $(targets)
.PHONY: all

override CPPFLAGS += $(gb_CPPFLAGS)
override CFLAGS   += $(gb_CFLAGS)
override LDLIBS   += $(gb_LDLIBS)
override LDFLAGS  += $(gb_LDFLAGS)

zz_core.o: override CPPFLAGS += $(PACKAGE_CPPFLAGS)

zz_exe_src = $(zz_out_src) $(zz_vfs_src) zingzong.c
zz_lib_src = $(zz_cor_src) $(zz_mix_src) $(zz_pla_src) $(zz_zzz_src)

zz_exe_obj = $(zz_exe_src:.c=.o)
zz_lib_obj = $(zz_lib_src:.c=.o)

zz_cor_src = $(addsuffix .c,$(cor))
zz_mix_src = $(addsuffix .c,$(mix))
zz_out_src = $(addsuffix .c,$(out))
zz_vfs_src = $(addsuffix .c,$(vfs))
zz_pla_src = $(addsuffix .c,$(pla))
zz_zzz_src = $(addsuffix .c,$(zzz))

zz_cor_obj = $(zz_cor_src:.c=.o)
zz_mix_obj = $(zz_mix_src:.c=.o)
zz_out_obj = $(zz_out_src:.c=.o)
zz_vfs_obj = $(zz_vfs_src:.c=.o)
zz_pla_obj = $(zz_pla_src:.c=.o)
zz_zzz_obj = $(zz_zzz_src:.c=.o)

mix := $(addprefix mix_,none lerp qerp soxr srate help test)
out := out_ao out_raw
vfs := vfs_file vfs_ice
cor := zz_init zz_core
pla := zz_play zz_log
zzz := $(addprefix zz_,load bin mem str vfs mixers)

sources = $(sort $(zz_exe_src) $(zz_lib_src))
headers = zingzong.h zz_private.h zz_def.h mix_common.c
objects = $(sources:.c=.o)

$(zz_exe): $(zz_lib_obj) $(zz_exe_obj)

clean_files += $(zz_lib) $(zz_exe)

# ----------------------------------------------------------------------
#  Distrib
# ----------------------------------------------------------------------

dist_top_lst = LICENSE README.md vcversion.sh
dist_src_lst = README.md Makefile $(sources) zz_fast.c $(headers)	\
make.clean make.depend make.dist make.info
dist_rsc_lst = zingzong.rc resource.h zingzong.ico
dist_amp_lst = README.md Makefile dialogs.c in_zingzong.c	\
in_zingzong.h in_zingzong.nsi
dist_68k_lst = README.md Makefile mksymb m68k_mem.S m68k_mix.S		\
m68k_muldiv.h mix_aga.c mix_ata.c mix_ata.h mix_fal.c mix_ste_dnr.c	\
mix_ste_hub.c mix_ste_lrb.c mix_ste_s7s.c mix_stf.c player.c sc68.x	\
sc68_debug.h sc68_start.S song.S stub.S test_sc68.s test_sndh.s		\
test_tos.s vset.S ym10_pack.h zz_m68k.c zz_m68k.h zz_tos.h zingzong.x

dist_lst = $(sort \
 $(addprefix ./,$(dist_top_lst)) \
 $(addprefix rsc/,$(dist_rsc_lst)) \
 $(addprefix src/,$(dist_src_lst)) \
 $(addprefix src/zz-winamp/,$(dist_amp_lst)) \
 $(addprefix src/zz-m68k/,$(dist_68k_lst)) )

include $(topdir)/src/make.dist

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

ifndef prefix
PREFIX = $(error 'prefix' must be set to install)
else
PREFIX = $(prefix)
endif

ifndef datadir
DATADIR = $(PREFIX)/share
else
DATADIR = $(datadir)
endif

exec_dir = $(PREFIX)
bindir   = $(exec_dir)/bin
libdir   = $(exec_dir)/lib

mandir  = $(DATADIR)/man
man1dir = $(mandir)/man1
man1ext = .1
docdir  = $(DATADIR)/doc/$(PACKAGE)-$(VERSION)
INSTALL = install $(INSTALL_OPT) -t "$(install_dir)"

install_dir = $(error install_dir must be defined)
INSTALL_OPT = -m644

bin_targets = $(targets)
doc_targets = $(topdir)/README.md
man_targets = $(srcdir)/man/zingzong.1

install-strip: INSTALL_STRIP = --strip-program=$(STRIP) -s
install-strip: install

install: install-exec install-data
	@echo "$(PACKAGE) $(VERSION) should be installed"

install-bin uninstall-bin: install_dir = $(DESTDIR)$(bindir)
install-bin: INSTALL_OPT = -m755 $(INSTALL_STRIP)
install-bin: $(bin_targets)
	@mkdir -p -- "$(install_dir)"
	$(INSTALL) $^

install-exec uninstall-exec: install-bin
install-data: install-man install-doc

install-man uninstall-man: install_dir = $(DESTDIR)$(man1dir)
install-doc uninstall-doc: install_dir = $(DESTDIR)$(docdir)

install-man: $(man_targets)
	@mkdir -p -- "$(install_dir)"
	$(foreach F,$^,	sed -e 's/@VERSION@/$(VERSION)/' "$(F)" \
	>"$(install_dir)/$(notdir $(F))")

install-doc: $(doc_targets)
	@mkdir -p -- "$(install_dir)"
	$(INSTALL) $<

.PHONY: install-strip install install-exec
.PHONY: install-data install-man install-doc

# ----------------------------------------------------------------------

uninstall-exec: uninstall-bin
uninstall-data: uninstall-man uninstall-doc
uninstall-bin: ; @cd "$(install_dir)" && rm -f -- $(notdir $(bin_targets))
uninstall-man: ; @cd "$(install_dir)" && rm -f -- $(notdir $(man_targets))
uninstall-doc: ; @rm -rf -- $(install_dir)

uninstall: uninstall-exec uninstall-data
	@echo "$(PACKAGE) $(VERSION) should have been uninstalled"

.PHONY: uninstall uninstall-exec uninstall-data uninstall-man uninstall-doc

define more-tools
endef

define more-help
	$(call Mline)
	$(call Minfo,install, Install all)
	$(call Minfo,install-strip, Install and strip programs)
	$(call Minfo,uninstall, Uninstall all)
	$(call Mline)
	$(call Minfo,distrib, Make distribution archive)
	$(call Minfo,distcheck, Make distribution archive and test it)
endef

# ----------------------------------------------------------------------
#  Includes
# ----------------------------------------------------------------------

include $(topdir)/src/make.info
include $(topdir)/src/make.clean
include $(topdir)/src/make.depend

ifdef MAKERULES
include $(MAKERULES)
endif
