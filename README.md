# cscript

`cscript.c` contains the source for a C "interpreter". Behind the scenes it
actually compiles the file and runs it immediately, so it's not _really_ an
interpreter. Point is, it allows you to use a shebang (just like `#!/bin/sh`) to
write C programs that execute like a Bash scripts.

## An example

Assuming you have cscript installed (use `make install`), create the file
`hello` containing:

```c
#!/usr/bin/env -S cscript -std=c11 -lm

#include <stdio.h>
#include <stdlib.h>

int main()
{
	puts("Hello, World!");

	return EXIT_SUCCESS;
}
```

Then run it with:

```sh
# make executable
chmod +x ./hello
# run
./hello
```

## Not just C!

It is possible to compile any other languages that your C compiler (specifically
`/bin/cc`) supports. The following shebang works for C++ files:

```cpp
#!/usr/bin/env -S cscript -xc++ -lc++

#include <iostream>
#include <cstdlib>

int main()
{
	std::cout << "This is C++!" << std::endl;

	return EXIT_SUCCESS;
}
```

Behind the scenes, cscript invokes `cc` with a couple of flags, including
`-x c`, followed by the flags specified on the shebang line. The `-x c++` in the
shebang line overwrites the `-x c` inserted by cscript, and the file is compiled
as a C++ source file. You could event do this for Fortan you are using GCC.

Note the `-S` switch. It instructs `env` to split the arguments into seperate
strings. This is important as some operating systems deliver all the shebang
arguments as a single argument.

## Don't use this

If you actually want to use this, a (slightly) more portable version would look
like this:

```c
//usr/bin/cc -o ${o=`mktemp`} "$0" && exec -a "$0" "$o" "$@"

#include <stdio.h>
#include <stdlib.h>

int main()
{
	puts("Hello, World!");

	return EXIT_SUCCESS;
}
```

If, for some reason, you still have to compile a file containing a cscript
shebang, do this:

```sh
# WRONG -- will complain about #!
cc -c x -o hello.exe hello

# correct -- ignores #! with `tail`
tail -n+2 hello | cc -x c -o hello.exe -
```

Also see `examples/slightly_portable.c` for an example of how to use cscript in
the same ways as the C example on this page.

## A note on shebang arguments

There's a pretty important difference between how the remainder of the shebang
line after the interpreter is handled. Linux passes everything after the
interpreter as a single argument, while Darwin splits the remaining arguments
like the shell does.

So something like the following invocation likely won't work across different
UNIX-like operations systems. See [the Portability section of the Wikipedia
article](shebangport) for more information.

```
#!cscript -std=c11 -Wall -Wextra
```

You can use `/usr/bin/env` with the `-S` flag to work around this issue.

[shebangport]: https://en.wikipedia.org/wiki/Shebang_(Unix)#Portability
