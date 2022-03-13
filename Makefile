### CONFIG ###

INSTALL:=/usr/local/bin

# reduce output
MAKEFLAGS:=--silent

### PHONY ###

.PHONY: test install uninstall clean

test: example.c
	./example.c 1 2 3

install: cscript
	mv cscript $(INSTALL)/cscript

uninstall:
	$(RM) $(INSTALL)/cscript

clean:
	$(RM) cscript

### RULES ###

cscript: cscript.c
	$(CC) -o $@ $<

example.c: cscript
