#!./cscript -std=c11

// This file uses the local version of cscript. Must be run from project root.

#include <err.h>
#include <sysexits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i) {
		fwrite(argv[i], strlen(argv[i]), 1, stdout);

		if (i != argc - 1)
			putchar(' ');
	}
	putchar('\n');

	return EXIT_SUCCESS;
}
