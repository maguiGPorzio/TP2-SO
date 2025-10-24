#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "processes.h"

// ============================================
//           DEFINICIONES
// ============================================

#define MIN_PRIORITY 0
#define MAX_PRIORITY 9
#define DEFAULT_PRIORITY 5

// ============================================
//           TIPOS DE DATOS
// ============================================

typedef struct SchedulerCDT *SchedulerADT;

// ============================================
//           FUNCIONES PÚBLICAS
// ============================================

// Inicialización
SchedulerADT init_scheduler(void);
SchedulerADT get_scheduler(void);
void scheduler_destroy(void);

// Función principal del scheduler (llamada por timer interrupt)
void *schedule(void *prev_rsp);

// Gestión de procesos
int scheduler_add_process(PCB *process);
int scheduler_remove_process(int pid);
int scheduler_set_priority(int pid, uint8_t priority);
int scheduler_get_priority(int pid);

// Bloqueo/desbloqueo (para usar desde processes.c)
int scheduler_block_process(int pid);
int scheduler_unblock_process(int pid);

// Control de scheduling
void scheduler_force_reschedule(void);
int scheduler_get_current_pid(void);

// Estadísticas
void scheduler_get_stats(uint64_t *total_ticks);
void scheduler_print_queues(void);  // Debug

#endif // SCHEDULER_H