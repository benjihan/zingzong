#! /usr/bin/make -f
#
# Makefile for zinzong
#

SHELL   = /bin/sh
srcdir := $(dir $(lastword $(MAKEFILE_LIST)))
VPATH   = $(srcdir)
PKGCONFIG=pkg-config
VERSION:=$(shell $(srcdir)vcversion.sh)

# ----------------------------------------------------------------------

ifndef AO_LIBS
AO_LIBS := $(shell $(PKGCONFIG) --libs ao || echo ERROR)
ifeq '$(AO_LIBS)' 'ERROR'
$(error Unable to configure libao with $(PKGCONFIG))
endif
AO_CLAGS := $(shell $(PKGCONFIG) --cflags ao)
endif

zingzong_CPPFLAGS := $(CPPFLAGS) -DPACKAGE_VERSION='"$(VERSION)"'
zingzong_CFLAGS   := $(CFLAGS) $(AO_CFLAGS)
zingzong_LDLIBS   := $(LDLIBS) $(AO_LIBS)

zingzafe_CPPFLAGS := $(CPPFLAGS) -DPACKAGE_VERSION='"$(VERSION)"'
zingzafe_CFLAGS   := $(CFLAGS)
zingzafe_LDLIBS   := $(LDLIBS)

# ----------------------------------------------------------------------

all: zingzong #zingzafe
clean: ; -rm -f zingzong zingzafe

zingzong: CPPFLAGS = $(zingzong_CPPFLAGS)
zingzong: CFLAGS   = $(zingzong_CFLAGS)
zingzong: LDLIBS   = $(zingzong_LDLIBS)

zingzafe: CPPFLAGS = $(zingzafe_CPPFLAGS)
zingzafe: CFLAGS   = $(zingzafe_CFLAGS)
zingzafe: LDLIBS   = $(zingzafe_LDLIBS)
