### CONFIG ###

INSTALL:=/usr/local/bin
NAME:=cscript

# reduce output
MAKEFLAGS:=--silent

### PHONY ###

.PHONY: test install uninstall clean

test: example.c
	./example.c 1 2 3

install: $(NAME)
	mv $(NAME) $(INSTALL)/$(NAME)

uninstall:
	$(RM) $(INSTALL)/$(NAME)

clean:
	@echo "you may also want to run \`make uninstall'"
	$(RM) $(NAME)

### RULES ###

$(NAME): cscript.c
	$(CC) -DNAME='"$(NAME)"' -o $@ $<

example.c: cscript
