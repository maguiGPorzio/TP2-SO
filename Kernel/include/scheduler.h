#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================
//         FORWARD DECLARATIONS
// ============================================

// Forward declaration - no necesitamos incluir processes.h
typedef struct PCB PCB;

// ============================================
//              CONSTANTES
// ============================================

#define MIN_PRIORITY 0      // Máxima prioridad (más tiempo de CPU)
#define MAX_PRIORITY 9      // Mínima prioridad (menos tiempo de CPU)
#define DEFAULT_PRIORITY 5

// ============================================
//              TIPOS DE DATOS
// ============================================

typedef struct SchedulerCDT *SchedulerADT;

// ============================================
//           INTERFAZ PÚBLICA
// ============================================

/**
 * @brief Inicializa el scheduler
 * @return ADT del scheduler, o NULL si falla
 */
SchedulerADT init_scheduler(void);

/**
 * @brief Obtiene el scheduler global
 * @return ADT del scheduler
 */
SchedulerADT get_scheduler(void);

/**
 * @brief Función principal del scheduler (llamada por timer interrupt)
 * @param prev_rsp Stack pointer del proceso anterior
 * @return Stack pointer del siguiente proceso a ejecutar
 */
void *schedule(void *prev_rsp);

/**
 * @brief Agrega un proceso al scheduler
 * @param process PCB del proceso a agregar
 * @return 0 si éxito, -1 si error
 */
int scheduler_add_process(PCB *process);

/**
 * @brief Remueve un proceso del scheduler
 * @param pid PID del proceso a remover
 * @return 0 si éxito, -1 si error
 */
int scheduler_remove_process(int pid);

/**
 * @brief Cambia la prioridad de un proceso
 * @param pid PID del proceso
 * @param priority Nueva prioridad (0-9)
 * @return 0 si éxito, -1 si error
 */
int scheduler_set_priority(int pid, uint8_t priority);

/**
 * @brief Obtiene la prioridad de un proceso
 * @param pid PID del proceso
 * @return Prioridad, o -1 si error
 */
int scheduler_get_priority(int pid);

/**
 * @brief Fuerza un reschedule (usado por yield)
 */
void scheduler_force_reschedule(void);

/**
 * @brief Obtiene el PID del proceso actual
 * @return PID actual, o -1 si no hay proceso
 */
int scheduler_get_current_pid(void);

/**
 * @brief Obtiene estadísticas del scheduler
 * @param total_ticks Puntero para guardar total de ticks
 */
void scheduler_get_stats(uint64_t *total_ticks);

/**
 * @brief Libera recursos del scheduler
 */
void scheduler_destroy(void);

#endif // SCHEDULER_H