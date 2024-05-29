include ../config.mk

IDIR:=.
BIN:=$(BUILD)/lib$(NAME).a

OBJS=$(ODIR)/$(NAME).o                         \
     $(ODIR)/$(NAME)/blocks.o                  \
     $(ODIR)/$(NAME)/tags.o                    \
     $(PLUGINS:%=$(ODIR)/$(NAME)/plugins/%.o)

override CFLAGS+= $(FLAGS) $(DEFINE) -fPIC -I$(IDIR)

$(BIN): $(OBJS) ; $(AR) rcs $@ $^

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > $@.txt
