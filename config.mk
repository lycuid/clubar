NAME=xdbar
BUILDDIR=bin
DEBUGDIR=debug

BIN=$(BUILDDIR)/$(NAME)
PREFIX=/usr/local
BINPREFIX=$(PREFIX)/bin

INC=-I/usr/include/freetype2 -Iinclude
LIBS=-lX11 -lfontconfig -lXft
CFLAGS=-Wall -pedantic
