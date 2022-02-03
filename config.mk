# constants.
NAME:=xdbar
MAIN:=$(NAME).c
VERSION:=0.2.1
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

ifneq ($(filter luaconfig,$(PATCHES)),)
PKGS+=lua
endif

FLAGS=-Wall -Wextra -pedantic -I. -O3 -ggdb -pthread -std=c99
DEFINE=-DNAME='"$(NAME)"' \
			 -DVERSION='"$(VERSION)"' \
			 -D_POSIX_C_SOURCE=200809 \
			 $(PATCHES:%=-DPatch_%)

# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
