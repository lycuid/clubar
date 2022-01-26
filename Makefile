include config.mk

$(BIN): $(MAIN) $(OBJS) config.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(MAIN) $(OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

.PHONY: run
run: $(BIN)
	$(BIN) $(ARGS)

.PHONY: debug
debug: $(BIN)
	gdb $(BIN)

.PHONY: options
options:
	@echo "$(NAME) build options:"
	@echo "CC       = $(CC)"
	@echo "PKGS     = $(PKGS)"
	@echo "OBJS     = $(OBJS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "----------------------------------"

.PHONY: clean
clean:
	$(RM) $(BIN) $(wildcard $(SRCDIRS:%=%/*.o))

.PHONY: fmt
fmt:
	clang-format -i $(MAIN) config.h
	clang-format -i $(wildcard $(SRCDIRS:%=%/*.c))
	clang-format -i $(wildcard $(SRCDIRS:%=%/*.h))

.PHONY: install
install: options $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/$(NAME)
