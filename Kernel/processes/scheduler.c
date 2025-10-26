#include "scheduler.h"
#include "process.h" 
#include <memoryManager.h>
#include "lib.h"
#include "ready_queue.h"

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================


typedef struct SchedulerCDT {
    PCB *processes[MAX_PROCESSES];              // Array para acceso por PID
    ReadyQueue ready_queue;                      // Cola de procesos READY (round-robin)
    int current_pid;                             // PID del proceso actual
    uint8_t process_count;                       // Cantidad total de procesos
    uint64_t total_cpu_ticks;                    // Total de ticks de CPU
    bool force_reschedule;                       // Flag para forzar cambio de proceso
} SchedulerCDT;

// ============================================
//         VARIABLES GLOBALES
// ============================================

static SchedulerADT scheduler = NULL;

// Variables compartidas con ASM (para context switching)
extern void *current_kernel_rsp;
extern void *switch_to_rsp;
extern void switch_to_rsp_and_iret(void *next_rsp);

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static PCB *pick_next_process(void);
static void cleanup_terminated_processes(void);

// Lista circular: use the ready_queue API (ready_queue_enqueue/dequeue/next)

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

    // Inicializar array de procesos
    for (int i = 0; i < MAX_PROCESSES; i++) {
        scheduler->processes[i] = NULL;
    }
    
    // Inicializar cola de procesos READY
    ready_queue_init(&scheduler->ready_queue);
    
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

void * schedule(void * prev_rsp) {
    if (scheduler == NULL) {
        return prev_rsp;
    }

    // ==========================================
    // 1. Guardar RSP y gestionar proceso actual
    // ==========================================
    if (scheduler->current_pid >= 0 && 
        scheduler->processes[scheduler->current_pid] != NULL) {
        
        PCB *current = scheduler->processes[scheduler->current_pid];
        current->stack_pointer = prev_rsp; // actualiza el rsp del proceso que estuvo corriendo hasta ahora en su pcb
        
        current->cpu_ticks++; // cuantas veces interrumpimos este proceso que estuvo corrriendo hasta ahora
        scheduler->total_cpu_ticks++;
        
        if (current->remaining_quantum > 0) { // si se lo interrumpió antes de que se termine su quantum
            current->remaining_quantum--;
        }
        
        // FIX #1: Re-encolar cuando pierde quantum
        if (current->status == PS_RUNNING) { // por si estaba bloqueado o terminado
            current->status = PS_READY;
            ready_queue_enqueue(&scheduler->ready_queue, current);
        }
    }

    // ==========================================
    // 2. Elegir siguiente proceso
    // ==========================================
    PCB *next = pick_next_process();
    
    //QUE PASA SI EL PROCESO ACTUAL ESTA BLOQUEADO O TERMINADO
    if (next == NULL) { // no hay proceso ready para correr
        scheduler->force_reschedule = false; // TODO: chequear esto, por que hace que corra el actual si por ahi esta bloqueado o terminated
        if (scheduler->current_pid >= 0 && 
            scheduler->processes[scheduler->current_pid] != NULL) {
            return scheduler->processes[scheduler->current_pid]->stack_pointer;
        }
        return prev_rsp;
    }

    // FIX #2: Desencolar proceso elegido
    ready_queue_dequeue(&scheduler->ready_queue, next);

    // ==========================================
    // 3. Cambiar al nuevo proceso
    // ==========================================
    scheduler->current_pid = next->pid;
    next->status = PS_RUNNING;
    next->remaining_quantum = next->priority;
    scheduler->force_reschedule = false;

    return next->stack_pointer;
}

// ============================================
//    ALGORITMO: ROUND ROBIN CON PRIORIDADES
//    (usando Lista Circular - O(10) vs O(N))
// ============================================

static PCB *pick_next_process(void) {
    if (scheduler == NULL || scheduler->process_count == 0) {
        return NULL;
    }
    
    ReadyQueue *queue = &scheduler->ready_queue;
    if (queue->count == 0) return NULL;
    PCB *candidate = ready_queue_next(queue);
    if (candidate != NULL && candidate->status == PS_READY) {
        return candidate;
    }

    return NULL;
}

// ============================================
//     OPERACIONES DE LISTA CIRCULAR
// ============================================

// Lista circular (enqueue/dequeue/next) movida a ready_queue.c

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

int scheduler_add_process(process_entry_t entry, int argc, const char **argv, const char *name) {
    if (scheduler == NULL || scheduler->process_count >= MAX_PROCESSES) {
        return -1;
    }

    // Buscar primer PID libre
    int pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler->processes[i] == NULL) {
            pid = i;
            break;
        }
    }
    if (pid < 0) {
        return -1;
    }

    PCB *process = proc_create(pid, entry, argc, argv, name);
    if (process == NULL) {
        return -1;
    }

    scheduler->processes[pid] = process;
    scheduler->process_count++;

    //TODO: asignar prioridad al proceso cuando se crea
    // Inicializar campos relacionados con scheduling
    process->priority = DEFAULT_PRIORITY; // Asignar prioridad por defecto
    process->status = PS_READY; // TODO: chequear si esto ya no lo hace proc_create
    process->cpu_ticks = 0;
    process->remaining_quantum = process->priority;
    if (process->parent_pid < 0) {
        process->parent_pid = scheduler->current_pid;
    }
    process->waiting_on = -1;

    ready_queue_enqueue(&scheduler->ready_queue, process);

    return pid;
}

int scheduler_remove_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }

    // Remover de la cola de su prioridad (si está en READY)
    if (process->status == PS_READY) {
        ready_queue_dequeue(&scheduler->ready_queue, process);
    }

    // Remover del array
    scheduler->processes[pid] = NULL;
    scheduler->process_count--;

    // Si estábamos ejecutando este proceso, forzar reschedule
    if (scheduler->current_pid == pid) {
        scheduler->current_pid = -1;
        scheduler_force_reschedule(); //TODO: VER ESTO
    }

    return 0;
}

int scheduler_set_priority(int pid, uint8_t new_priority) {
   if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES || 
        scheduler->processes[pid] == NULL || 
        new_priority < MIN_PRIORITY || new_priority > MAX_PRIORITY) {
        return -1;
    }

    scheduler->processes[pid]->priority = new_priority;
    scheduler->processes[pid]->remaining_quantum = new_priority;
    return 0;
}

int scheduler_get_priority(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES || scheduler->processes[pid] == NULL) {
        return -1;
    }
    PCB *process = scheduler->processes[pid];
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

// TODO: preguntar si esta bien la idea (porque podria volver a ser elegido)
void scheduler_yield(void) {
    scheduler_force_reschedule(); // TODO: no me cierra la idea de force_reschedule y que no se pueda elegir de nuevo al mismo proceso
}

//TODO: LEERLA porque la hizo claude
int scheduler_kill_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    PCB *proc = scheduler->processes[pid];
    if (proc == NULL) {
        return -1;                      // no existe
    }
    if (proc->status == PS_TERMINATED) {
        return 0;                       // ya estaba terminado
    }

    // Si estaba en READY, sacarlo de la cola global para que no sea elegido
    if (proc->status == PS_READY) {
        ready_queue_dequeue(&scheduler->ready_queue, proc);
    }

    // Marcar como TERMINATED (la liberación real la hace cleanup_terminated_processes)
    proc->status = PS_TERMINATED;

    // Si era el proceso actual, forzar replanificación inmediata
    if (pid == scheduler->current_pid) {
        scheduler->current_pid = -1;    // ya no hay "actual"
        scheduler_force_reschedule();
    }

    return 0;
}


// ============================================
//   FUNCIONES AUXILIARES PARA BLOQUEO
// ============================================

// Llamar desde proc_block() en processes.c
int scheduler_block_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }
    
    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }
    
    // Remover de cola READY (si está ahí)
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        ready_queue_dequeue(&scheduler->ready_queue, process);
    }
    
    process->status = PS_BLOCKED;
    
    // Si es el proceso actual, forzar reschedule
    if (pid == scheduler->current_pid) {
        scheduler_force_reschedule();
    }
    
    return 0;
}

//Llamar desde proc_unblock() en processes.c
//NOSE SI CUANDO SE DESBLOQUEA SE TENDRIA QUE FORZAR EL RESCHEDULE PORQUE QUIZAS TENGA MAS PRIORIDAD DE CORRER QUE OTROS
int scheduler_unblock_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }
    
    PCB *process = scheduler->processes[pid];
    if (process == NULL || process->status != PS_BLOCKED) {
        return -1;
    }
    
    process->status = PS_READY;
    
    // Agregar a la cola READY global
    ready_queue_enqueue(&scheduler->ready_queue, process);
    
    return 0;
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
            // Si tiene padre, no lo liberamos aún (zombie) hasta que el padre haga wait()
            if (p->parent_pid >= 0) {
                continue;
            }
            // Remover de cola si estuviera encolado (seguro aunque no esté)
            ready_queue_dequeue(&scheduler->ready_queue, p);
            
            // Liberar recursos del proceso
            free_process_resources(p);
            scheduler->processes[i] = NULL;
            scheduler->process_count--;
        }
    }
}

void scheduler_destroy(void) {
    if (scheduler == NULL) {
        return;
    }

    MemoryManagerADT mm = getKernelMemoryManager();

    // Limpiar la cola global liberando nodos
    ready_queue_destroy(&scheduler->ready_queue);

    // No liberamos los PCBs aquí, eso lo hace cleanup_all_processes()
    freeMemory(mm, scheduler);
    scheduler = NULL;
}

// En scheduler.c
PCB *scheduler_get_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return NULL;
    }
    
    return scheduler->processes[pid];
}

// ============================================
//   TERMINACIÓN INMEDIATA DEL PROCESO ACTUAL
// ============================================
//todo: M ELA HIZO CLAUDE NO LA LEI
void scheduler_exit_process(int64_t retValue) {
    if (scheduler == NULL) {
        // No scheduler available; halt CPU
        while (1) { __asm__ __volatile__("hlt"); }
    }

    int cur = scheduler->current_pid;
    if (cur < 0 || cur >= MAX_PROCESSES) {
        // No current process; nothing sensible to switch to
        while (1) { __asm__ __volatile__("hlt"); }
    }

    PCB *proc = scheduler->processes[cur];
    if (proc != NULL) {
        proc->return_value = (int)retValue;
        proc->status = PS_TERMINATED;
        // Si el padre está esperando por este hijo, desbloquearlo
        if (proc->parent_pid >= 0) {
            PCB *parent = scheduler_get_process(proc->parent_pid);
            if (parent != NULL && parent->status == PS_BLOCKED && parent->waiting_on == proc->pid) {
                parent->waiting_on = -1;
                scheduler_unblock_process(parent->pid);
            }
        }
        // Por seguridad, intentar removerlo de la cola si estuviera encolado (no hace nada si no lo está)
        ready_queue_dequeue(&scheduler->ready_queue, proc);
    }

    // Ya no hay proceso actual activo
    scheduler->current_pid = -1;
    scheduler_force_reschedule();

    // Capturar el RSP actual como prev para que schedule() seleccione el próximo
    void *prev_rsp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(prev_rsp));

    void *next_rsp = schedule(prev_rsp);

    // Si no hay siguiente proceso, detener
    if (next_rsp == NULL || next_rsp == prev_rsp) {
        while (1) { __asm__ __volatile__("hlt"); }
    }

    // Cambio de contexto inmediato (no retorna)
    switch_to_rsp_and_iret(next_rsp);

    // No debería alcanzarse
    while (1) { __asm__ __volatile__("hlt"); }
}

// Bloquea al proceso actual hasta que el hijo indicado termine. Devuelve el status del hijo si ya terminó o 0.
int scheduler_wait(int child_pid) {
    if (scheduler == NULL || child_pid < 0 || child_pid >= MAX_PROCESSES) {
        return -1;
    }
    int cur = scheduler->current_pid;
    if (cur < 0) return -1;
    PCB *child = scheduler->processes[child_pid];
    if (child == NULL) return -1;
    if (child->parent_pid != cur) return -1; // solo se permite esperar hijos directos

    // Si el hijo ya terminó, reaper y devolver status
    if (child->status == PS_TERMINATED) {
        int ret = child->return_value;
        // liberar recursos del hijo
        free_process_resources(child);
        scheduler->processes[child_pid] = NULL;
        scheduler->process_count--;
        return ret;
    }

    // Bloquear al padre y hacer un cambio de contexto inmediato
    PCB *parent = scheduler->processes[cur];
    if (parent == NULL) return -1;
    parent->waiting_on = child_pid;
    scheduler_block_process(cur);

    // Preparar cambio de contexto a otro proceso inmediatamente desde syscall
    extern void *current_kernel_rsp;
    extern void *switch_to_rsp;
    void *next_rsp = schedule(current_kernel_rsp);
    switch_to_rsp = next_rsp;

    // Valor de retorno cuando vuelva a ejecutarse; tests lo ignoran
    return 0;
}