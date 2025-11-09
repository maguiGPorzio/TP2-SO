#include "usrlib.h"
#include "test_util.h"

#define TOTAL_PROCESSES 3

#define LOWEST 2  // TODO: Change as required
#define MEDIUM 1 // TODO: Change as required
#define HIGHEST 0 // TODO: Change as required

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

uint64_t max_value = 0;

void zero_to_max() {
  uint64_t local_max = max_value;  // Copia local para evitar lecturas repetidas
  uint64_t value = 0; 

  while (value++ != local_max);

  printf("PROCESS %d DONE!\n", sys_getpid());
}

int test_prio(int argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  const char *ztm_argv[] = {0};
  uint64_t i;

  if (argc != 1) {
    print_err("Error: test_prio requires exactly 1 argument\n");
    print_err("Usage: test prio <max_iterations>\n");
    print_err("  max_iterations: number to count up to (affects duration)\n");
    print_err("Example: test prio 100000000\n");
    return -1;
  }

  if ((max_value = satoi(argv[0])) <= 0) {
    print_err("Error: invalid max_iterations value ");
    print_err(argv[0]);
    print_err("\nmax_iterations must be a positive integer\n");
    return -1;
  }

  printf("SAME PRIORITY...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++)
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max", NULL);

  // Expect to see them finish at the same time

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max", NULL);
    sys_nice(pids[i], prio[i]);
    printf("  PROCESS %d NEW PRIORITY: %d\n", pids[i], prio[i]);
  }

  // Expect the priorities to take effect

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max", NULL);
    sys_block(pids[i]);
    sys_nice(pids[i], prio[i]);
    printf("  PROCESS %d NEW PRIORITY: %d\n", pids[i], prio[i]);
  }

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_unblock(pids[i]);

  // Expect the priorities to take effect

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_wait(pids[i]);

  return 0;
}