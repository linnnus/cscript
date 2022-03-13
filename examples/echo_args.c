#!./cscript -std=c11 -lm

// This file uses the local version of cscript. Must be run from project root.

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	puts("I recieved the following arguments:");
	for (int i = 0; i < argc; ++i)
		printf("argv[%d] = `%s'\n", i, argv[i]);

	return EXIT_SUCCESS;
}
