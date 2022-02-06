#!./cscript -std=c11 -lm

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	puts("I recieved the following arguments:");
	for (int i = 0; i < argc; ++i)
		printf("argv[%d] = `%s'\n", i, argv[i]);

	return EXIT_SUCCESS;
}
