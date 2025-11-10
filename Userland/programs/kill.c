// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int kill_main(int argc, char *argv[])
{
	if (argc == 0) {
		print_err("Usage: kill <pid1> [pid2] [pid3] ...\n");
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

		int result = sys_kill(pid);

		if (result == 0) {
			printf("Process %d killed successfully\n", pid);
		} else {
			switch (result) {
			case -1:
				printf("Error: Scheduler not initialized\n");
				break;
			case -2:
				printf("Error: Invalid PID %d\n", pid);
				break;
			case -3:
				printf("Error: Process %d not found\n", pid);
				break;
			case -4:
				printf("Error: Cannot kill PID %d (protected process)\n", pid);
				break;
			default:
				printf("Error: Failed to kill process %d\n", pid);
				break;
			}
			errors++;
		}
	}

	return (errors == 0) ? OK : ERROR;
}