NAME=xdbar
BUILDDIR=bin
DEBUGDIR=debug
BIN=$(BUILDDIR)/$(NAME)
DEBUGBIN=$(DEBUGDIR)/$(NAME)

PREFIX=/usr/local
BINPREFIX=$(PREFIX)/bin

CFLAGS=-Wall -Wextra -pedantic -O3
LDFLAGS=-lpthread -lX11 -lfontconfig -lXft
INC=-I/usr/include/freetype2
OBJS=blocks.o x.o

$(BIN): $(NAME).c config.h $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INC) -o $(BIN) $(NAME).c *.o

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

%.o: include/%.c include/%.h
	$(CC) $(LDFLAGS) $(INC) $(CFLAGS) -c $<

options:
	@echo "$(NAME) build options:"
	@echo "CC       = $(CC)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "INC      = $(INC)"

run: $(BIN)
	$(BIN)

install: options $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

uninstall:
	rm $(DESTDIR)$(BINPREFIX)/$(NAME)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(DEBUGDIR) *.o

.PHONY: dbg
dbg:
	mkdir -p $(DEBUGDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INC) -g -o $(DEBUGBIN) $(NAME).c include/*.c

.PHONY: debug
debug: dbg
	gdb $(DEBUGDIR)/$(NAME)
