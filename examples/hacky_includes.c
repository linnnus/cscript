#!./cscript

#include <stdio.h>
#include <stdlib.h>

// Relative includes work like normally. Here, we use that to include all of the
// logic from a library.
//
// This is very useful when writing small scripts to test functionality from a
// much larger application. You can just #include the files you want to test
// directly.
#include "./some_lib.c"

int main(void) {
	int a = 3;
	int b = 5;
	printf("%d + %d = %d\n", a, b, add(a, b));
	return EXIT_SUCCESS;
}
