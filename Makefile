include config.mk

$(BIN): $(NAME).c $(OBJS) config.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(NAME).c $(OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

run: $(BIN)
	$(BIN) $(ARGS)

debug: $(BIN)
	gdb $(BIN)

.PHONY: options
options:
	@echo "$(NAME) build options:"
	@echo "CC       = $(CC)"
	@echo "PKGS     = $(PKGS)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"

.PHONY: clean
clean:
	$(RM) $(BIN) $(OBJS) $(LUAOBJ)

.PHONY: fmt
fmt:
	clang-format -i $(NAME).c config.h include/*.c include/*.h

.PHONY: install
install: options $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/$(NAME)
