NAME:=clubar
VERSION:=0.4.6
BUILD:=.build
ODIR:=$(BUILD)/cache
IDIR:=src
BIN:=$(BUILD)/bin/$(NAME)
FRONTEND:=$(NAME)/frontend
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin
MANPREFIX:=$(PREFIX)/man/man1

PKGS=x11 xft
PLUGINS=
SRC=$(IDIR)/$(NAME).c                        \
    $(IDIR)/$(NAME)/core.c                   \
    $(IDIR)/$(NAME)/core/blocks.c            \
    $(IDIR)/$(NAME)/core/tags.c              \
    $(PLUGINS:%=$(IDIR)/$(NAME)/plugins/%.c)

OBJS=$(SRC:$(IDIR)/%.c=$(ODIR)/%.o)
ifneq ($(filter luaconfig,$(PLUGINS)),)
	PKGS+= lua
endif

FLAGS:=-Wall -Wextra -Wvla -pedantic -I$(IDIR) -Ofast -ggdb -std=c99
DEFINE:=-D_GNU_SOURCE                       \
        -DNAME='"$(NAME)"'                  \
        -DVERSION='"$(VERSION)"'            \
        $(PLUGINS:%=-D__ENABLE_PLUGIN__%__)

# In case 'pkg-config' is not installed, update LDFLAGS and CFLAGS accordingly.
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=-lpthread $(shell pkg-config --libs $(PKGS))
