// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int echo_main(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++) {
		print(argv[i]);
		putchar(' ');
	}
	putchar(EOF);
	return OK;
}