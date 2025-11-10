// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int block_main(int argc, char *argv[])
{
	if (argc == 0) {
		print("Usage: block <pid1> [pid2] [pid3] ...\n");
		return ERROR;
	}

	int errors = 0;

	for (int i = 0; i < argc; i++) {
		int pid = satoi(argv[i]);

		if (pid < 0) {
			printf("Invalid PID: %s\n", argv[i]);
			errors++;
			continue;
		}

		int result = sys_block(pid);

		if (result == OK) {
			printf("Process %d blocked successfully\n", pid);
		} else {
			printf("Error: Failed to block process %d\n", pid);
			errors++;
		}
	}

	return (errors == 0) ? OK : ERROR;
}