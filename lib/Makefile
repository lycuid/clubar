include ../config.mk

I_DIR:=.
BIN:=$(BUILD)/lib$(NAME).a

O_FILES=$(O_DIR)/$(NAME).o                         \
        $(O_DIR)/$(NAME)/blocks.o                  \
        $(O_DIR)/$(NAME)/tags.o                    \
        $(PLUGINS:%=$(O_DIR)/$(NAME)/plugins/%.o)

override CFLAGS+= $(FLAGS) $(DEFINE) -fPIC -I$(I_DIR)

all: $(BIN)

$(BIN): $(O_FILES) ; $(AR) rcs $@ $^

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > $@.txt
