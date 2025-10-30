#include "usrlib.h"
#include "test_util.h"

#define TOTAL_PROCESSES 3

#define LOWEST 1  // TODO: Change as required
#define MEDIUM 3  // TODO: Change as required
#define HIGHEST 5 // TODO: Change as required

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

uint64_t max_value = 0;

void zero_to_max() {
  uint64_t value = 0; 

  while (value++ != max_value);

  printf("PROCESS %d DONE!\n", sys_getpid());
}

uint64_t test_prio(uint64_t argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  const char *ztm_argv[] = {0};
  uint64_t i;

  if (argc != 1)
    return -1;

  if ((max_value = satoi(argv[0])) <= 0)
    return -1;

  printf("SAME PRIORITY...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++)
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max");

  // Expect to see them finish at the same time

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max");
    sys_nice(pids[i], prio[i]);
    printf("  PROCESS %d NEW PRIORITY: %d\n", pids[i], prio[i]);
  }

  // Expect the priorities to take effect

  for (i = 0; i < TOTAL_PROCESSES; i++)
    sys_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = sys_create_process(&zero_to_max, 0, ztm_argv, "zero_to_max");
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