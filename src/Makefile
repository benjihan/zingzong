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

# ----------------------------------------------------------------------

all: zingzong
clean: ; -rm -f zingzong

zingzong: CFLAGS += $(AO_CFLAGS) -DPACKAGE_VERSION='"$(VERSION)"'
zingzong: LDLIBS += $(AO_LIBS)
