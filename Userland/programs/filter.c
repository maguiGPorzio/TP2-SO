// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

static int is_vowel(char c)
{
	return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'A' || c == 'E' ||
	        c == 'I' || c == 'O' || c == 'U');
}

int filter_main(int argc, char *argv[])
{
	if (argc != 0) {
		print_err("Filter requires no arguments\n");
		return ERROR;
	}

	char c;

	while ((c = getchar()) != EOF) {
		if (!is_vowel(c)) {
			putchar(c);
		}
	}

	return OK;
}