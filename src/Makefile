#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
#
#
# ----------------------------------------------------------------------

SHELL=/usr/bin/sh
PKGCONFIG:=pkg-config

# ----------------------------------------------------------------------

ifndef AO_LIBS
AO_LIBS := $(shell $(PKGCONFIG) --libs ao || echo ERROR)
ifeq '$(AO_LIBS)' 'ERROR'
$(error Unable to configure libao with $(PKGCONFIG))
endif
AO_CLAGS := $(shell $(PKGCONFIG) --cflags ao)
endif

# ----------------------------------------------------------------------

all: zingzong
clean: ; -rm -f zingzong

zingzong: CFLAGS += $(AO_CFLAGS)
zingzong: LDLIBS += $(AO_LIBS)
