# config
INSTALL:=/usr/local/bin
NAME:=cscript

# reduce output
MAKEFLAGS:=--silent

.PHONY: test install uninstall clean

test: $(NAME)
	./examples/echo.c Hello, Worlscd
	./examples/echo_args.c arg1 arg2 arg2
	./examples/install_check.c

install: $(NAME)
	mv $(NAME) $(INSTALL)/$(NAME)

uninstall:
	$(RM) $(INSTALL)/$(NAME)

clean:
	@echo "you may also want to run \`make uninstall'"
	$(RM) $(NAME)

$(NAME): cscript.c
	$(CC) -DNAME='"$(NAME)"' -o $@ $<
