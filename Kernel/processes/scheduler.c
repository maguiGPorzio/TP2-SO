#include "scheduler.h"
#include "processes.h" 
#include <memoryManager.h>
#include "lib.h"

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================

typedef struct SchedulerCDT {
    PCB *processes[MAX_PROCESSES];  // Array de PCBs
    int current_pid;                 // PID del proceso actual
    uint8_t process_count;           // Cantidad de procesos
    uint64_t total_cpu_ticks;        // Total de ticks de CPU
    bool force_reschedule;           // Flag para forzar cambio
} SchedulerCDT;

// ============================================
//         VARIABLES GLOBALES
// ============================================

static SchedulerADT scheduler = NULL;

// Variables compartidas con ASM (para context switching)
extern void *current_kernel_rsp;
extern void *switch_to_rsp;

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static PCB *pick_next_process(void);
static void cleanup_terminated_processes(void);

// ============================================
//           INICIALIZACIÓN
// ============================================

SchedulerADT init_scheduler(void) {
    if (scheduler != NULL) {
        return scheduler;
    }

    MemoryManagerADT mm = getKernelMemoryManager();
    scheduler = allocMemory(mm, sizeof(SchedulerCDT));
    
    if (scheduler == NULL) {
        return NULL;
    }

    // Inicializar estructura
    for (int i = 0; i < MAX_PROCESSES; i++) {
        scheduler->processes[i] = NULL;
    }
    
    scheduler->current_pid = -1;
    scheduler->process_count = 0;
    scheduler->total_cpu_ticks = 0;
    scheduler->force_reschedule = false;

    return scheduler;
}

SchedulerADT get_scheduler(void) {
    return scheduler;
}

// ============================================
//        SCHEDULER PRINCIPAL
// ============================================

void *schedule(void *prev_rsp) {
    if (scheduler == NULL || scheduler->process_count == 0) {
        return prev_rsp;
    }

    cleanup_terminated_processes();

    // 1. Guardar RSP del proceso actual
    if (scheduler->current_pid >= 0 && 
        scheduler->processes[scheduler->current_pid] != NULL) {
        
        PCB *current = scheduler->processes[scheduler->current_pid];
        current->stack_pointer = prev_rsp;
        
        // Incrementar ticks de CPU
        current->cpu_ticks++;
        scheduler->total_cpu_ticks++;
        
        // Decrementar quantum
        if (current->remaining_quantum > 0) {
            current->remaining_quantum--;
        }
        
        // Marcar como READY si estaba RUNNING
        if (current->status == PS_RUNNING) {
            current->status = PS_READY;
        }
    }

    // 2. Elegir siguiente proceso
    PCB *next = pick_next_process();
    
    if (next == NULL) {
        // No hay procesos READY, retornar al actual
        if (scheduler->current_pid >= 0 && 
            scheduler->processes[scheduler->current_pid] != NULL) {
            return scheduler->processes[scheduler->current_pid]->stack_pointer;
        }
        return prev_rsp;
    }

    // 3. Cambiar al nuevo proceso
    scheduler->current_pid = next->pid;
    next->status = PS_RUNNING;
    
    // Resetear quantum basado en prioridad
    next->remaining_quantum = next->priority + 1;
    
    // Resetear flag de force_reschedule
    scheduler->force_reschedule = false;

    return next->stack_pointer;
}

// ============================================
//    ALGORITMO: ROUND ROBIN CON PRIORIDADES
// ============================================

static PCB *pick_next_process(void) {
    if (scheduler == NULL || scheduler->process_count == 0) {
        return NULL;
    }

    PCB *current = NULL;
    if (scheduler->current_pid >= 0) {
        current = scheduler->processes[scheduler->current_pid];
    }

    // Si el proceso actual aún tiene quantum Y no se forzó reschedule
    if (current != NULL && 
        current->status == PS_READY && 
        current->remaining_quantum > 0 &&
        !scheduler->force_reschedule) {
        return current;
    }

    // Buscar proceso con mayor prioridad (menor número)
    PCB *best_candidate = NULL;
    uint8_t highest_priority = MAX_PRIORITY + 1;
    int best_idx = -1;

    // Primera pasada: encontrar la prioridad más alta entre procesos READY
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB *p = scheduler->processes[i];
        if (p != NULL && p->status == PS_READY && p->priority < highest_priority) {
            highest_priority = p->priority;
        }
    }

    // Segunda pasada: Round Robin entre procesos de esa prioridad
    // Empezar desde el siguiente al actual
    int start = (scheduler->current_pid >= 0) ? 
                (scheduler->current_pid + 1) % MAX_PROCESSES : 0;

    for (int k = 0; k < MAX_PROCESSES; k++) {
        int idx = (start + k) % MAX_PROCESSES;
        PCB *p = scheduler->processes[idx];
        
        if (p != NULL && 
            p->status == PS_READY && 
            p->priority == highest_priority) {
            best_candidate = p;
            best_idx = idx;
            break;
        }
    }

    return best_candidate;
}

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

int scheduler_add_process(PCB *process) {
    if (scheduler == NULL || process == NULL) {
        return -1;
    }

    if (scheduler->process_count >= MAX_PROCESSES) {
        return -1;
    }

    int pid = process->pid;
    if (pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    if (scheduler->processes[pid] != NULL) {
        return -1;  // Slot ya ocupado
    }

    scheduler->processes[pid] = process;
    scheduler->process_count++;

    // Inicializar campos relacionados con scheduling
    process->status = PS_READY;
    process->cpu_ticks = 0;
    process->remaining_quantum = process->priority + 1;

    return 0;
}

int scheduler_remove_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    if (scheduler->processes[pid] == NULL) {
        return -1;
    }

    scheduler->processes[pid] = NULL;
    scheduler->process_count--;

    // Si estábamos ejecutando este proceso, resetear current
    if (scheduler->current_pid == pid) {
        scheduler->current_pid = -1;
    }

    return 0;
}

int scheduler_set_priority(int pid, uint8_t priority) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        return -1;
    }

    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }

    process->priority = priority;
    process->remaining_quantum = priority + 1;

    return 0;
}

int scheduler_get_priority(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }

    return process->priority;
}

void scheduler_force_reschedule(void) {
    if (scheduler != NULL) {
        scheduler->force_reschedule = true;
    }
}

int scheduler_get_current_pid(void) {
    if (scheduler == NULL) {
        return -1;
    }
    return scheduler->current_pid;
}

void scheduler_get_stats(uint64_t *total_ticks) {
    if (scheduler != NULL && total_ticks != NULL) {
        *total_ticks = scheduler->total_cpu_ticks;
    }
}

// ============================================
//        LIMPIEZA DE PROCESOS
// ============================================

static void cleanup_terminated_processes(void) {
    if (scheduler == NULL) {
        return;
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        // Saltear proceso actual
        if (i == scheduler->current_pid) {
            continue;
        }

        PCB *p = scheduler->processes[i];
        if (p != NULL && p->status == PS_TERMINATED) {
            // Liberar recursos (llamar a función de processes.c)
            // scheduler->processes[i] = NULL ya se hace en proc_exit
        }
    }
}

void scheduler_destroy(void) {
    if (scheduler == NULL) {
        return;
    }

    MemoryManagerADT mm = getKernelMemoryManager();
    
    // No liberamos los PCBs aquí, eso lo hace cleanup_all_processes()
    freeMemory(mm, scheduler);
    scheduler = NULL;
}