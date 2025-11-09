// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "../include/usrlib.h"
#include "test_util.h"

enum State { RUNNING,
             BLOCKED,
             KILLED };

typedef struct P_rq {
  int32_t pid;
  enum State state;
} p_rq;

int test_processes(int argc, char *argv[]) {
  uint8_t rq;
  uint8_t alive = 0;
  uint8_t action;
  uint64_t max_processes;
  const char *argvAux[] = {0};

  if (argc != 1) {
    print_err("Error: test_processes requires exactly 1 argument\n");
    print_err("Usage: test processes <max_processes>\n");
    print_err("  max_processes: number of processes to create and manage\n");
    print_err("Example: test processes 10\n");
    return -1;
  }

  if ((max_processes = satoi(argv[0])) <= 0) {
    printf("Error: invalid max_processes value ");
    print_err(argv[0]);
    printf("\nmax_processes must be a positive integer\n");
    return -1;
  }

  p_rq p_rqs[max_processes];

  while (1) {

    // Create max_processes processes
    for (rq = 0; rq < max_processes; rq++) {
      p_rqs[rq].pid = sys_create_process(&endless_loop, 0, argvAux, "endless_loop", NULL);

      if (p_rqs[rq].pid == -1) {
        print_err("test_processes: ERROR creating process\n");
        return -1;
      } else {
        p_rqs[rq].state = RUNNING;
        alive++;
      }
    }

    // Randomly kills, blocks or unblocks processes until every one has been killed
    while (alive > 0) {

      for (rq = 0; rq < max_processes; rq++) {
        action = get_uniform(100) % 2;

        switch (action) {
          case 0:
            if (p_rqs[rq].state == RUNNING || p_rqs[rq].state == BLOCKED) {
              if (sys_kill(p_rqs[rq].pid) == -1) {
                print_err("test_processes: ERROR killing process\n");
                return -1;
              }
              sys_wait(p_rqs[rq].pid); // lo agregue yo asi se le liberan los recursos al pobre hombre
              p_rqs[rq].state = KILLED;
              alive--;
            }
            break;

          case 1:
            if (p_rqs[rq].state == RUNNING) {
              if (sys_block(p_rqs[rq].pid) == -1) {
                print_err("test_processes: ERROR blocking process\n");
                return -1;
              }
              p_rqs[rq].state = BLOCKED;
            }
            break;
        }
      }

      // Randomly unblocks processes
      for (rq = 0; rq < max_processes; rq++)
        if (p_rqs[rq].state == BLOCKED && get_uniform(100) % 2) {
          if (sys_unblock(p_rqs[rq].pid) == -1) {
            print_err("test_processes: ERROR unblocking process\n");
            return -1;
          }
          p_rqs[rq].state = RUNNING;
        }
    }
  }
}