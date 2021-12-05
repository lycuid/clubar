NAME=xdbar
BUILDDIR=bin
DEBUGDIR=debug

BIN=$(BUILDDIR)/$(NAME)
PREFIX=/usr/local
BINPREFIX=$(PREFIX)/bin

INC=-I/usr/include/freetype2 -Iinclude
LIBS=-lX11 -lfontconfig -lXft
CFLAGS=-Wall -pedantic

build: clean include.o
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(LIBS) $(INC) -o $(BIN) $(NAME).c *.o

include.o:
	$(CC) $(CFLAGS) -c include/*.c

.PHONY: dbg
dbg:
	mkdir -p $(DEBUGDIR)
	$(CC) $(CFLAGS) $(LIBS) $(INC) -ggdb -o $(DEBUGDIR)/$(NAME) $(NAME).c include/*.c

.PHONY: debug
debug: dbg
	gdb $(DEBUGDIR)/$(NAME)

run: build
	$(BIN)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(DEBUGDIR) *.o

install: $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

uninstall:
	rm $(DESTDIR)$(BINPREFIX)/$(NAME)
