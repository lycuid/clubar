include config.mk

BIN:=$(BUILD)/bin/$(NAME)
PREFIX:=/usr/local
BINPREFIX:=$(PREFIX)/bin
MANPREFIX:=$(PREFIX)/man/man1

.PHONY: with_x11 lib
with_x11: ; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -C src/$@
	cp src/$@/$(BIN) $(BIN)

lib: ; $(MAKE) -j -C lib

.PHONY: install uninstall
install: $(BIN)
	mkdir -p $(DESTDIR)$(BINPREFIX)
	strip $(BIN)
	cp -f $(BIN) $(DESTDIR)$(BINPREFIX)/$(NAME)
	sed -e 's/APPNAME/$(NAME)/g' -e 's/APPVERSION/$(VERSION)/g' $(NAME).1.tmpl \
		| gzip > $(DESTDIR)$(MANPREFIX)/$(NAME).1.gz
	chmod 755 $(DESTDIR)$(BINPREFIX)/$(NAME)
	chmod 644 $(DESTDIR)$(MANPREFIX)/$(NAME).1.gz

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/$(NAME)

.PHONY: fmt run debug clean compile_flags
fmt: ; @git ls-files | grep -E '\.[ch]$$' | xargs clang-format -i
run: $(BIN) ; $(BIN) $(ARGS)
debug: $(BIN) ; @gdb $(BIN)
clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C src/with_x11 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C src/with_x11 $@
