NAME:=clubar
VERSION:=0.5.1
BUILD:=.build
BIN:=$(BUILD)/bin/$(NAME)
ODIR:=$(BUILD)/cache

PLUGINS=
FLAGS:=-Wall -Wextra -Wvla -pedantic -Ofast -ggdb
DEFINE:=-D_GNU_SOURCE                       \
        -DNAME='"$(NAME)"'                  \
        -DVERSION='"$(VERSION)"'            \
        $(PLUGINS:%=-D__ENABLE_PLUGIN__%__)
