#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdint.h>

/* ===== Constantes solo usadas por process.c ===== */
#define MAX_NAME_LENGTH     32
#define PROCESS_STACK_SIZE  4096  /* 4 KB */

/* ===== Tipos ===== */
typedef enum {
    PS_READY = 0,
    PS_BLOCKED,
    PS_RUNNING,
    PS_TERMINATED
} process_status_t;

typedef int (*process_entry_t)(int argc, char **argv);

typedef struct PCB {
    int pid;
    process_status_t status;
    char name[MAX_NAME_LENGTH];

    void *stack_base;
    void *stack_pointer;

    process_entry_t entry;
    char **argv;
    int argc;

    uint8_t  priority;          /* 0-9 (0 = mayor prioridad) */
    uint8_t  remaining_quantum; /* ticks restantes del turno */
    uint64_t cpu_ticks;         /* ticks de CPU usados */

    int return_value;
} PCB;

/* ===== API expuesta por process.c ===== */
PCB *proc_spawn(int pid,
                process_entry_t entry,
                int argc,
                const char **argv,
                const char *name);

#endif /* PROCESSES_H */
