# config
INSTALL:=/usr/local/bin

# reduce output
MAKEFLAGS:=--silent

.PHONY: test install uninstall clean

test: cscript
	./examples/echo.c Hello, Worlscd
	./examples/echo_args.c arg1 arg2 arg2
	./examples/install_check.c

install: cscript
	mv cscript $(INSTALL)/cscript

uninstall:
	$(RM) $(INSTALL)/cscript

# NOTE: you may also want to run `make uninstall`
clean:
	$(RM) cscript

cscript: cscript.c
	$(CC) -o $@ $<
