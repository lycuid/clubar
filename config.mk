# constants.
NAME:=xdbar
MAIN:=$(NAME).c
VERSION:=0.3.0
BUILDDIR:=bin
BIN:=$(BUILDDIR)/$(NAME)
# install paths.
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin

# misc.
PKGS=x11 xft
PLUGINS=
SRCDIRS=include include/patches
OBJS=include/blocks.o \
		 include/x.o \
		 include/utils.o \
		 $(PLUGINS:%=include/plugins/%.o)

ifneq ($(filter luaconfig,$(PLUGINS)),)
PKGS+=lua
endif

FLAGS=-Wall -Wextra -pedantic -I. -O3 -ggdb -pthread -std=c99
DEFINE=-DNAME='"$(NAME)"' \
			 -DVERSION='"$(VERSION)"' \
			 -D_POSIX_C_SOURCE=200809 \
			 $(PLUGINS:%=-D__ENABLE_PLUGIN__%__)

# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
