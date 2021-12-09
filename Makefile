NAME=xdbar
BUILDDIR=bin
DEBUGDIR=debug

BIN=$(BUILDDIR)/$(NAME)
PREFIX=/usr/local
BINPREFIX=$(PREFIX)/bin

INC=-I/usr/include/freetype2 -Iinclude
LDFLAGS=-lX11 -lpthread -lfontconfig -lXft
CFLAGS=-Wall -pedantic -O3

build: clean include.o
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INC) -o $(BIN) $(NAME).c *.o

options:
	@echo xdbar build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "INC      = $(INC)"
	@echo "CC       = $(CC)"

include.o:
	$(CC) $(CFLAGS) -c include/*.c

.PHONY: dbg
dbg:
	mkdir -p $(DEBUGDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INC) -ggdb -o $(DEBUGDIR)/$(NAME) $(NAME).c include/*.c

.PHONY: debug
debug: dbg
	gdb $(DEBUGDIR)/$(NAME)

run: build
	$(BIN)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(DEBUGDIR) *.o

install: options $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

uninstall:
	rm $(DESTDIR)$(BINPREFIX)/$(NAME)
