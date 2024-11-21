# config
INSTALL := /usr/local/bin

.PHONY: test install uninstall clean
.DEFAULT_GOAL := cscript

test: cscript
	./examples/echo.c Hello, World
	./examples/echo_args.c arg1 arg2 arg2
	./examples/hacky_includes.c

installtest:
	./examples/install_check.c

install: cscript
	mv cscript $(INSTALL)/cscript

uninstall:
	$(RM) $(INSTALL)/cscript

clean:
	@echo "NOTE: you may also want to run \`make uninstall\`"
	$(RM) cscript cscript.dSYM

cscript: cscript.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
