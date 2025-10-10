// Kernel/include/scheduler.h
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Estados de un proceso
typedef enum {
    READY,      // Listo para ejecutar
    RUNNING,    // Ejecutando actualmente
    BLOCKED,    // Bloqueado (esperando I/O, semáforo, etc.)
    KILLED      // Terminado/killed
} ProcessState;

// Prioridad de procesos
typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH = 2
} ProcessPriority;

// Process ID
typedef int64_t pid_t;

// Estructura de un proceso (PCB - Process Control Block)
typedef struct Process {
    pid_t pid;                      // Process ID
    char name[32];                  // Nombre del proceso
    ProcessState state;             // Estado actual
    ProcessPriority priority;       // Prioridad
    
    // Context (registros guardados)
    uint64_t rsp;                   // Stack pointer
    uint64_t rbp;                   // Base pointer
    uint64_t rip;                   // Instruction pointer
    
    // Stack del proceso
    void* stack_base;               // Base del stack
    size_t stack_size;              // Tamaño del stack
    
    // Información de scheduling
    uint64_t quantum;               // Quantum restante
    uint64_t total_cpu_time;        // Tiempo total de CPU usado
    
    // Proceso padre/hijo
    pid_t parent_pid;               // PID del padre
    
    // Lista enlazada
    struct Process* next;           // Siguiente en la cola
    struct Process* prev;           // Anterior en la cola
    
    // Foreground/Background
    bool is_foreground;             // ¿Es proceso foreground?
} Process;

// Funciones públicas del scheduler
void scheduler_init(void);
void scheduler_start(void);
pid_t create_process(void (*entry_point)(void), char* name, ProcessPriority priority, bool is_foreground);
void kill_process(pid_t pid);
void block_process(pid_t pid);
void unblock_process(pid_t pid);
void yield(void);
Process* get_current_process(void);
pid_t get_current_pid(void);
void schedule(void);
void print_processes(void);

// Funciones para el timer
void scheduler_tick(void);

#endif