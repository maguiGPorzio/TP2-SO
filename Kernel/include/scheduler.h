#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

// ============================================
//           DEFINICIONES
// ============================================

typedef int pid_t;  

#define MIN_PRIORITY 1
#define MAX_PRIORITY 5
#define DEFAULT_PRIORITY 5

// ============================================
//           FUNCIONES PÚBLICAS
// ============================================

// Inicialización
int init_scheduler(void);
void scheduler_destroy(void);

// Función principal del scheduler (llamada por timer interrupt)
void *schedule(void *prev_rsp);

// Gestión de procesos
int scheduler_add_process(process_entry_t entry, int argc, const char **argv, const char *name, int fds[2]);
int scheduler_remove_process(int pid);
int scheduler_set_priority(int pid, uint8_t priority);
int scheduler_get_priority(int pid);
void scheduler_yield(void);
int scheduler_kill_process(int pid);
PCB *scheduler_get_process(int pid);
void scheduler_exit_process(int64_t retValue);
int scheduler_waitpid(int child_pid);

// Bloqueo/desbloqueo (para usar desde processes.c)
int scheduler_block_process(int pid);
int scheduler_unblock_process(int pid);

// Control de scheduling
void scheduler_force_reschedule(void);
int scheduler_get_current_pid(void);

// Información de procesos
int scheduler_get_processes(process_info_t *buffer, int max_count);

#endif // SCHEDULER_H
