// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int red_main(int argc, char *argv[])
{
	char c;
	while ((c = getchar()) != EOF) {
		sys_write(STDERR, &c, 1);
	}

	return OK;
}