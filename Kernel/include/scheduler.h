#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

// ============================================
//           DEFINICIONES
// ============================================

typedef int pid_t;  

#define MAX_PRIORITY 0
#define DEFAULT_PRIORITY 1
#define MIN_PRIORITY 2
#define PRIORITY_COUNT 3

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
int scheduler_remove_process(pid_t pid);
int scheduler_set_priority(pid_t pid, uint8_t priority);
int scheduler_get_priority(pid_t pid);
void scheduler_yield(void);
int scheduler_kill_process(pid_t pid);
PCB *scheduler_get_process(pid_t pid);
void scheduler_exit_process(int64_t retValue);
int scheduler_waitpid(pid_t child_pid);

// Bloqueo/desbloqueo (para usar desde processes.c)
int scheduler_block_process(pid_t pid);
int scheduler_unblock_process(pid_t pid);

// Control de scheduling
void scheduler_force_reschedule(void);
int scheduler_get_current_pid(void);

// Información de procesos
int scheduler_get_processes(process_info_t *buffer, int max_count);

// Foreground process control (getter/setter)
pid_t scheduler_get_foreground_pid(void);
int scheduler_set_foreground_process(pid_t pid);
int scheduler_kill_foreground_process(void);

// Reparent single process to INIT
int adopt_init_as_parent(pid_t pid);

#endif // SCHEDULER_H
