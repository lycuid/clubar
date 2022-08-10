include config.mk

.PHONY: x11 wayland
x11: $(OBJS) $(ODIR)/$(NAME)/x.o
	mkdir -p $(shell dirname $(BIN)) && $(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $^

# @TODO: Not Implemented.
wayland: $(OBJS) $(ODIR)/$(NAME)/wayland.o
	mkdir -p $(shell dirname $(BIN)) && $(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $^

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h
	mkdir -p $(@D) && $(CC) $(CFLAGS) -c -o $@ $<
$(ODIR)/%.o: $(IDIR)/%.c
	mkdir -p $(@D) && $(CC) $(CFLAGS) -c -o $@ $<
$(OBJS): $(IDIR)/config.h

.PHONY: options
options:
	@echo "$(NAME) build options:"
	@echo "CC       = $(CC)"
	@echo "PKGS     = $(PKGS)"
	@echo "PLUGINS  = $(PLUGINS)"
	@echo "SRC      = $(SRC)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "----------------------------------"

.PHONY: install uninstall
install: options $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp -f $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/$(NAME)

# misc.
.PHONY: fmt clean debug run
fmt: ; @git ls-files | egrep '\.[ch]$$' | xargs clang-format -i
clean: ; rm -rf $(BUILD)
run: $(BIN) ; $(BIN) $(ARGS)
debug: $(BIN) ; gdb $(BIN)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
