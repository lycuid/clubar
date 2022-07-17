NAME:=xdbar
VERSION:=0.3.1
BUILD=.build
ODIR=$(BUILD)/cache
BIN:=$(ODIR)/bin/$(NAME)
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin

PKGS=x11 xft
PLUGINS=
SRC=$(NAME).c          \
     include/blocks.c  \
     include/x.c       \
     include/utils.c

SRC+=$(PLUGINS:%=include/plugins/%.c)
OBJS=$(SRC:%.c=$(ODIR)/%.o)

ifneq ($(filter luaconfig,$(PLUGINS)),)
PKGS+=lua
endif

FLAGS:=-Wall -Wextra -pedantic -I. -O3 -ggdb -pthread -std=c99
DEFINE:=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' -D_POSIX_C_SOURCE=200809
DEFINE+= $(PLUGINS:%=-D__ENABLE_PLUGIN__%__)
# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
