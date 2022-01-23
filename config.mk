# constants.
NAME:=xdbar
VERSION:=0.1.0
BUILDDIR:=bin
BIN:=$(BUILDDIR)/$(NAME)
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin

# misc.
CONFIG_TYPE=0
DEFINE=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' \
			 -Dlua=1 -DCONFIG_TYPE=$(CONFIG_TYPE)
LUAOBJ=include/lua.o
OBJS=include/blocks.o include/x.o include/utils.o

PKGS=x11 xft
FLAGS=-Wall -Wextra -pedantic -O3 -ggdb -pthread

ifeq ($(CONFIG_TYPE), lua)
PKGS+=lua
OBJS+=$(LUAOBJ)
endif

# In case 'pkg-config' is not installed, update these variables accordingly.
CFLAGS+=$(DEFINE) $(FLAGS) $(shell pkg-config --cflags $(PKGS))
LDFLAGS+=$(shell pkg-config --libs $(PKGS))
