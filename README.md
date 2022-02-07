# cscript

`cscript.c` contains the source for a C "interpreter". Behind the scenes it
actually compiles the file and runs it immediatle. Point is, it allows one to
write C programs as though they where shell scripts:

```c
#!/usr/local/bin/csript -std=c11 -lm

#include <stdio.h>

int main()
{

}
```

## Don't use this

If you actually want to use this, a (slightly) more protable version would look
like this:

```c
//usr/bin/cc -o ${o=`mktemp`} "$0" && exec -a "$0" "$o" "$@"

#include <stdio.h>

int main()
{
	puts("Hello, World!");

	return 0;
}
```
