include config.mk

BIN:=$(BUILD)/bin/$(NAME)
PREFIX:=/usr/local
FRONTEND:=frontend
BINPREFIX:=$(PREFIX)/bin
MANPREFIX:=$(PREFIX)/man/man1

.PHONY: frontend/x11 lib
$(FRONTEND)/x11: lib; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -C $@
	cp $@/$(BIN) $(BIN)

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
	$(MAKE) -C $(FRONTEND)/x11 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C $(FRONTEND)/x11 $@
