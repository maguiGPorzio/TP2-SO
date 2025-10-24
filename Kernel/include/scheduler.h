#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration (evitar dependencia circular)
typedef struct PCB PCB;

// ============================================
//              CONSTANTES
// ============================================

#define MIN_PRIORITY 0   // Prioridad máxima (más importante)
#define MAX_PRIORITY 9   // Prioridad mínima (menos importante)
#define DEFAULT_PRIORITY 5  // Prioridad por defecto

// ============================================
//              TIPOS DE DATOS
// ============================================

/**
 * @brief Handle opaco del scheduler (ADT)
 */
typedef struct SchedulerCDT *SchedulerADT;

// ============================================
//           INICIALIZACIÓN
// ============================================

/**
 * @brief Inicializa el scheduler (singleton)
 * @return Puntero al scheduler, o NULL si falla
 */
SchedulerADT init_scheduler(void);

/**
 * @brief Obtiene la instancia del scheduler
 * @return Puntero al scheduler, o NULL si no está inicializado
 */
SchedulerADT get_scheduler(void);

/**
 * @brief Destruye el scheduler y libera sus recursos
 * @warning No libera los PCBs (eso lo hace processes.c)
 */
void scheduler_destroy(void);

// ============================================
//        SCHEDULER PRINCIPAL
// ============================================

/**
 * @brief Función principal de scheduling (llamada desde timer interrupt)
 * 
 * Realiza:
 * 1. Guarda el estado del proceso actual (RSP, ticks, quantum)
 * 2. Ajusta prioridad dinámicamente según comportamiento
 * 3. Aplica aging cada AGING_INTERVAL ticks
 * 4. Elige el siguiente proceso a ejecutar
 * 5. Retorna el RSP del proceso elegido
 * 
 * @param prev_rsp RSP del proceso actual (viene de pushState en ASM)
 * @return RSP del siguiente proceso a ejecutar
 */
void *schedule(void *prev_rsp);

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

/**
 * @brief Agrega un proceso al scheduler
 * 
 * El proceso debe haber sido creado previamente con proc_spawn().
 * Esta función lo registra en el scheduler y lo marca como READY.
 * 
 * @param process Puntero al PCB del proceso
 * @return 0 si OK, -1 si error
 */
int scheduler_add_process(PCB *process);

/**
 * @brief Remueve un proceso del scheduler
 * 
 * NO libera la memoria del PCB (eso lo hace processes.c).
 * Solo lo quita de las estructuras internas del scheduler.
 * 
 * @param pid PID del proceso a remover
 * @return 0 si OK, -1 si error
 */
int scheduler_remove_process(int pid);

// ============================================
//        GESTIÓN DE PRIORIDADES
// ============================================

/**
 * @brief Establece la prioridad de un proceso manualmente
 * 
 * Override del ajuste dinámico. Útil para:
 * - Tests (test_priority)
 * - Procesos críticos del sistema
 * - Syscall nice()
 * 
 * @param pid PID del proceso
 * @param priority Nueva prioridad (0-9, 0=máxima)
 * @return 0 si OK, -1 si error
 */
int scheduler_set_priority(int pid, uint8_t priority);

/**
 * @brief Obtiene la prioridad actual de un proceso
 * 
 * @param pid PID del proceso
 * @return Prioridad (0-9), o -1 si error
 */
int scheduler_get_priority(int pid);

// ============================================
//        CONTROL DE SCHEDULING
// ============================================

/**
 * @brief Fuerza un reschedule inmediato
 * 
 * Activa un flag para que pick_next_process() ignore
 * el quantum restante del proceso actual y elija otro.
 * 
 * Usado en:
 * - proc_exit() (el proceso termina, debe cambiar)
 * - Bloqueo de procesos (el proceso no puede seguir)
 */
void scheduler_force_reschedule(void);

/**
 * @brief Obtiene el PID del proceso actualmente en ejecución
 * 
 * @return PID del proceso actual, o -1 si no hay ninguno
 */
int scheduler_get_current_pid(void);

/**
 * @brief Obtiene estadísticas del scheduler
 * 
 * @param total_ticks [out] Total de ticks de CPU del sistema
 */
void scheduler_get_stats(uint64_t *total_ticks);

#endif // SCHEDULER_H