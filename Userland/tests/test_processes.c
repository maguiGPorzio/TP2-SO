#include "../include/usrlib.h"


static uint32_t m_z = 362436069;
static uint32_t m_w = 521288629;

uint32_t GetUint() {
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  return (m_z << 16) + m_w;
}

uint32_t GetUniform(uint32_t max) {
  uint32_t u = GetUint();
  return (u + 1.0) * 2.328306435454494e-10 * max;
}

int64_t satoi(char *str) {
  uint64_t i = 0;
  int64_t res = 0;
  int8_t sign = 1;

  if (!str)
    return 0;

  if (str[i] == '-') {
    i++;
    sign = -1;
  }

  for (; str[i] != '\0'; ++i) {
    if (str[i] < '0' || str[i] > '9')
      return 0;
    res = res * 10 + str[i] - '0';
  }

  return res * sign;
}


enum State { RUNNING,
             BLOCKED,
             KILLED };

typedef struct P_rq {
  int32_t pid;
  enum State state;
} p_rq;

int64_t test_processes(uint64_t argc, char *argv[]) {
  uint8_t rq;
  uint8_t alive = 0;
  uint8_t action;
  uint64_t max_processes;
  char *argvAux[] = {0};

  if (argc != 1)
    return -1;

  if ((max_processes = satoi(argv[0])) <= 0)
    return -1;

  p_rq p_rqs[max_processes];

  while (1) {

    // Create max_processes processes
    for (rq = 0; rq < max_processes; rq++) {
      p_rqs[rq].pid = my_create_process("endless_loop", 0, argvAux);

      if (p_rqs[rq].pid == -1) {
        printf("test_processes: ERROR creating process\n");
        return -1;
      } else {
        p_rqs[rq].state = RUNNING;
        alive++;
      }
    }

    // Randomly kills, blocks or unblocks processes until every one has been killed
    while (alive > 0) {

      for (rq = 0; rq < max_processes; rq++) {
        action = GetUniform(100) % 2;

        switch (action) {
          case 0:
            if (p_rqs[rq].state == RUNNING || p_rqs[rq].state == BLOCKED) {
              if (my_kill(p_rqs[rq].pid) == -1) {
                printf("test_processes: ERROR killing process\n");
                return -1;
              }
              p_rqs[rq].state = KILLED;
              alive--;
            }
            break;

          case 1:
            if (p_rqs[rq].state == RUNNING) {
              if (my_block(p_rqs[rq].pid) == -1) {
                printf("test_processes: ERROR blocking process\n");
                return -1;
              }
              p_rqs[rq].state = BLOCKED;
            }
            break;
        }
      }

      // Randomly unblocks processes
      for (rq = 0; rq < max_processes; rq++)
        if (p_rqs[rq].state == BLOCKED && GetUniform(100) % 2) {
          if (my_unblock(p_rqs[rq].pid) == -1) {
            printf("test_processes: ERROR unblocking process\n");
            return -1;
          }
          p_rqs[rq].state = RUNNING;
        }
    }
  }
}

// // Proceso de prueba: imprime 'a' y retorna 0
// int proc_print_a(int argc, char **argv) {
//     (void)argc; (void)argv;
//     putchar('a');
//     putchar('\n');
//     return 0;
// }
