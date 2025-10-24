// Simple processes API and PCB definition
#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================
//              CONSTANTES
// ============================================

#define MAX_PROCESSES      32
#define MAX_NAME_LENGTH    32
#define NO_PID             (-1)
#define INIT_PID           (0)
#define PROCESS_STACK_SIZE (4096)  // 4 KB

// Macro para validar si un PID está fuera de rango
#define IS_INVALID_PID(pid) ((pid) < 0 || (pid) >= MAX_PROCESSES)

// ============================================
//              TIPOS DE DATOS
// ============================================

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
    
    uint8_t priority;           // 0-9 (0 = máxima prioridad)
    uint8_t remaining_quantum;  // Ticks restantes en este turno
    uint64_t cpu_ticks;         // Total de ticks de CPU usados
    
    int return_value;
} PCB;

// ============================================
//           INTERFAZ PÚBLICA
// ============================================

/**
 * @brief Inicializa el sistema de procesos y ejecuta la shell
 */
void init_processes(void);

/**
 * @brief Inicializa el sistema de procesos SIN crear la shell (para tests)
 */
void init_processes_for_test(void);

/**
 * @brief Crea un nuevo proceso
 * @param entry Función de entrada del proceso
 * @param argc Cantidad de argumentos
 * @param argv Array de argumentos (strings)
 * @param name Nombre del proceso
 * @return PID del proceso creado, o -1 en caso de error
 */
int proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name);

/**
 * @brief Termina el proceso actual
 * @param status Código de retorno del proceso
 */
void proc_exit(int status);

/**
 * @brief Obtiene el PID del proceso actual
 * @return PID del proceso en ejecución
 */
int proc_getpid(void);

/**
 * @brief Cede voluntariamente la CPU al siguiente proceso (cooperativo)
 */
void proc_yield(void);

/**
 * @brief Bloquea un proceso específico
 * @param pid PID del proceso a bloquear, o -1 para bloquear el proceso actual
 * @return 0 en caso de éxito, -1 en caso de error
 */
int proc_block(int pid);

/**
 * @brief Desbloquea un proceso específico y lo marca como READY
 * @param pid PID del proceso a desbloquear
 * @return 0 en caso de éxito, -1 en caso de error
 */
int proc_unblock(int pid);

/**
 * @brief Imprime información de todos los procesos
 */
void proc_print(void);

/**
 * @brief Limpia TODOS los procesos (para tests y shutdown)
 * @warning Esta función libera recursos de todos los procesos, incluyendo el actual
 */
void cleanup_all_processes(void);

#endif // PROCESSES_H