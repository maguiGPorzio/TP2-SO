#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "queue.h"

#define MAX_PROCESSES 64
#define MAX_PROCESS_NAME_LENGTH 32
#define PROCESS_STACK_SIZE (4096 * 2)  // 8KB stack
#define MAX_PID (MAX_PROCESSES - 1)
#define KILLED_RET_VALUE -1

#define INIT_PID 0 
#define SHELL_PID 1
#define NO_PID -1

#define OK 0
#define ERROR -1

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
    int parent_pid;              // PID del proceso padre (-1 si no tiene)
    char name[MAX_PROCESS_NAME_LENGTH];
    
    // Estado y scheduling
    ProcessStatus status;
    uint8_t priority;                // 0-9 (0 = mayor prioridad)
    uint64_t last_tick;
    
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
    int waiting_on;                  // PID que está esperando (-1 si ninguno)

    // file descriptors
    int read_fd;
    int write_fd;
    bool killable;                   // si false, el proceso no puede ser matado (init/shell)

    // queues 
    queue_t open_fds; // lista de fds abiertos
} PCB;


// Estructura para exponer información de procesos a userland
typedef struct process_info {
    int pid;
    char name[MAX_PROCESS_NAME_LENGTH];
    ProcessStatus status;
    uint8_t priority;
    int parent_pid;
    int read_fd;
    int write_fd;
    uint64_t stack_base;
    uint64_t stack_pointer;
} process_info_t;


// ============================================
//      FUNCIONES INTERNAS (process.c)
// ============================================

// Creación y limpieza (usadas por scheduler)
PCB *proc_create(int pid, process_entry_t entry, int argc, const char **argv, const char *name, bool killable, int fds[2]);
void free_process_resources(PCB *p);

#endif // PROCESS_H