# constants.
NAME:=xdbar
MAIN:=$(NAME).c
VERSION:=0.2.0
BUILDDIR:=bin
BIN:=$(BUILDDIR)/$(NAME)
# install paths.
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin

# misc.
PKGS=x11 xft
PATCHES=
SRCDIRS=include include/patches
OBJS=include/blocks.o \
		 include/x.o \
		 include/utils.o \
		 $(PATCHES:%=include/patches/%.o)

ifneq ($(filter lua,$(PATCHES)),)
PKGS+=lua
endif

FLAGS=-Wall -Wextra -pedantic -I. -O3 -ggdb -pthread
DEFINE=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' $(PATCHES:%=-DPatch_%)

# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
