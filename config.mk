NAME:=xdbar
VERSION:=0.3.2
BUILD=.build
ODIR=$(BUILD)/cache
IDIR=src
BIN:=$(BUILD)/bin/$(NAME)
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin

PKGS=x11 xft
PLUGINS=
SRC=$(IDIR)/$(NAME).c             \
    $(IDIR)/$(NAME)/core.c        \
    $(IDIR)/$(NAME)/core/blocks.c \
    $(IDIR)/$(NAME)/x.c

SRC+=$(PLUGINS:%=$(IDIR)/$(NAME)/plugins/%.c)
OBJS=$(SRC:$(IDIR)/%.c=$(ODIR)/%.o)

PLUGIN_PKGS=$(filter luaconfig,$(PLUGINS))
PKGS+= $(PLUGIN_PKGS:luaconfig=lua)

FLAGS:=-Wall -Wextra -pedantic -I$(IDIR) -O3 -ggdb -std=c99
DEFINE:=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' -D_GNU_SOURCE
DEFINE+= $(PLUGINS:%=-D__ENABLE_PLUGIN__%__)
# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
