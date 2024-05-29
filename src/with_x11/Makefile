include ../../config.mk

IDIR:=.
LIB:=../../lib
OBJS:=$(ODIR)/main.o $(ODIR)/gui.o

PKGS:=x11 xft
ifneq ($(filter luaconfig,$(PLUGINS)),)
	PKGS+= lua
endif

override CFLAGS+= $(FLAGS) $(DEFINE) -I$(LIB) $(shell pkg-config --cflags $(PKGS))
LDFLAGS:=-lpthread $(shell pkg-config --libs $(PKGS))

$(BIN): $(LIB) $(OBJS) ; mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) -L$(LIB)/$(BUILD) -l$(NAME)

.PHONY: $(LIB)
$(LIB):
	$(MAKE) -j -C $@

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/%.o: $(IDIR)/%.c ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > $@.txt
