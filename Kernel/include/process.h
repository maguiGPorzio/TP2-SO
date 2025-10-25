#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES 64
#define MAX_NAME_LENGTH 32
#define PROCESS_STACK_SIZE (4096 * 2)  // 8KB stack

// ============================================
//           TIPOS Y ESTRUCTURAS
// ============================================

typedef int (*process_entry_t)(int argc, char **argv);

// Estados de proceso
typedef enum {
    PS_READY = 0,
    PS_RUNNING,
    PS_BLOCKED,
    PS_TERMINATED
} ProcessStatus;

// Process Control Block
typedef struct PCB {
    // Identificación
    int pid;
    char name[MAX_NAME_LENGTH];
    
    // Estado y scheduling
    ProcessStatus status;
    uint8_t priority;                // 0-9 (0 = mayor prioridad)
    uint8_t remaining_quantum;       // Ticks restantes en este quantum
    
    // Contexto de ejecución
    void *stack_base;                // Base del stack
    void *stack_pointer;             // RSP actual (apunta al contexto guardado)
    
    // Función de entrada
    process_entry_t entry;
    int argc;
    char **argv;
    
    // Estadísticas
    uint64_t cpu_ticks;              // Total de ticks de CPU usados
    int return_value;                // Valor de retorno (para exit)
    
    // ⭐ CRÍTICO: Lista circular (para scheduler) ⭐
    struct PCB *next;                // Siguiente en cola circular
    struct PCB *prev;                // Anterior en cola circular
    
} PCB;


// ============================================
//      FUNCIONES INTERNAS (process.c)
// ============================================

// Creación y limpieza (usadas por scheduler)
PCB *proc_create(int pid, process_entry_t entry, int argc, const char **argv, const char *name);
void free_process_resources(PCB *p);

#endif // PROCESS_H