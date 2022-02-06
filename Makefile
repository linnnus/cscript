INSTALL:=/usr/local/bin

.PHONY: test install uninstall clean

test: example.c
	./example.c 1 2 3

install: cscript
	mv cscript $(INSTALL)

uninstall:
	$(RM) $(INSTALL)/cscript

clean:
	$(RM) cscript

cscript: cscript.c
	$(CC) -o $@ $<

example.c: cscript
