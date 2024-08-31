include ../../config.mk

I_DIR:=.
L_DIR:=../../lib
O_FILES:=$(O_DIR)/main.o $(O_DIR)/gui.o

PKGS:=x11 xft
ifneq ($(filter luaconfig,$(PLUGINS)),)
	PKGS+= lua
endif

override CFLAGS+= $(FLAGS) $(DEFINE) -I$(LIB) $(shell pkg-config --cflags $(PKGS))
LDFLAGS:=-L$(LIB)/$(BUILD) -l$(NAME) -lpthread $(shell pkg-config --libs $(PKGS))

all: $(BIN)

$(BIN): $(LIB) $(O_FILES) ; mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $(O_FILES) $(LDFLAGS)

.PHONY: $(LIB)
$(LIB):
	$(MAKE) -j -C $@

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(O_DIR)/%.o: $(I_DIR)/%.c ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > $@.txt
