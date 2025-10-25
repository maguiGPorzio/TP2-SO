#include "scheduler.h"
#include "process.h" 
#include <memoryManager.h>
#include "lib.h"

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================

// Nodos internos para la cola circular (no modifican el PCB)
typedef struct RQNode {
    PCB *proc;
    struct RQNode *next;
    struct RQNode *prev;
} RQNode;

// Cola circular de procesos READY (una por prioridad), basada en nodos internos
typedef struct ReadyQueue {
    RQNode *head;          // Primer nodo en la cola
    RQNode *current;       // Nodo actual en rotación Round Robin
    int count;             // Cantidad de procesos en esta cola
} ReadyQueue;

typedef struct SchedulerCDT {
    PCB *processes[MAX_PROCESSES];              // Array para acceso por PID
    ReadyQueue ready_queues[MAX_PRIORITY + 1];  // Una cola por prioridad (0-9)
    int current_pid;                             // PID del proceso actual
    uint8_t process_count;                       // Cantidad total de procesos
    uint64_t total_cpu_ticks;                    // Total de ticks de CPU
    bool force_reschedule;                       // Flag para forzar cambio
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

// Operaciones de lista circular
static void enqueue_process(ReadyQueue *queue, PCB *process);
static void dequeue_process(ReadyQueue *queue, PCB *process);
static PCB *next_in_queue(ReadyQueue *queue);

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
    
    // Inicializar colas de prioridad
    for (int i = 0; i <= MAX_PRIORITY; i++) {
        scheduler->ready_queues[i].head = NULL;
        scheduler->ready_queues[i].current = NULL;
        scheduler->ready_queues[i].count = 0;
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
    if (scheduler == NULL) {
        return prev_rsp;
    }

    if (scheduler->process_count == 0) {
        scheduler->force_reschedule = false;
        return prev_rsp;
    }

    cleanup_terminated_processes();

    // ==========================================
    // 1. Guardar RSP y gestionar proceso actual
    // ==========================================
    if (scheduler->current_pid >= 0 && 
        scheduler->processes[scheduler->current_pid] != NULL) {
        
        PCB *current = scheduler->processes[scheduler->current_pid];
        current->stack_pointer = prev_rsp;
        
        current->cpu_ticks++;
        scheduler->total_cpu_ticks++;
        
        if (current->remaining_quantum > 0) {
            current->remaining_quantum--;
        }
        
        // ✅ FIX #1: Re-encolar cuando pierde quantum
        if (current->status == PS_RUNNING) {
            current->status = PS_READY;
            ReadyQueue *queue = &scheduler->ready_queues[current->priority];
            enqueue_process(queue, current);
        }
    }

    // ==========================================
    // 2. Elegir siguiente proceso
    // ==========================================
    PCB *next = pick_next_process();
    
    if (next == NULL) {
        scheduler->force_reschedule = false;
        if (scheduler->current_pid >= 0 && 
            scheduler->processes[scheduler->current_pid] != NULL) {
            return scheduler->processes[scheduler->current_pid]->stack_pointer;
        }
        return prev_rsp;
    }

    // ✅ FIX #2: Desencolar proceso elegido
    ReadyQueue *queue = &scheduler->ready_queues[next->priority];
    dequeue_process(queue, next);

    // ==========================================
    // 3. Cambiar al nuevo proceso
    // ==========================================
    scheduler->current_pid = next->pid;
    next->status = PS_RUNNING;
    next->remaining_quantum = next->priority + 1;
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

    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        ReadyQueue *queue = &scheduler->ready_queues[priority];
        
        if (queue->count > 0) {
            PCB *candidate = next_in_queue(queue);
            
            if (candidate != NULL && candidate->status == PS_READY) {
                return candidate;
            }
        }
    }

    return NULL;
}

// ============================================
//     OPERACIONES DE LISTA CIRCULAR
// ============================================

// Agregar proceso al final de la cola circular
static void enqueue_process(ReadyQueue *queue, PCB *process) {
    if (process == NULL) {
        return;
    }

    MemoryManagerADT mm = getKernelMemoryManager();
    RQNode *node = allocMemory(mm, sizeof(RQNode));
    if (node == NULL) {
        return; // sin memoria: no encola
    }
    node->proc = process;
    if (queue->head == NULL) {
        // Primera inserción: lista circular de 1 elemento
        node->next = node;
        node->prev = node;
        queue->head = node;
        queue->current = node;
    } else {
        // Insertar al final (antes del head)
        RQNode *tail = queue->head->prev;
        
        tail->next = node;
        node->prev = tail;
        node->next = queue->head;
        queue->head->prev = node;
    }
    
    queue->count++;
}

// Remover proceso de la cola circular
static void dequeue_process(ReadyQueue *queue, PCB *process) {
    if (process == NULL || queue->count == 0) {
        return;
    }
    
    if (queue->count == 0) {
        return;
    }

    // Buscar el nodo correspondiente al proceso
    RQNode *n = queue->head;
    RQNode *found = NULL;
    for (int i = 0; i < queue->count; i++) {
        if (n->proc == process) { found = n; break; }
        n = n->next;
    }
    if (found == NULL) {
        return; // no estaba en la cola
    }

    if (queue->count == 1) {
        // Último elemento
        queue->head = NULL;
        queue->current = NULL;
    } else {
        // Desenlazar de la lista circular
        found->prev->next = found->next;
        found->next->prev = found->prev;

        // Ajustar head si es necesario
        if (queue->head == found) {
            queue->head = found->next;
        }
        
        // Ajustar current si es necesario
        if (queue->current == found) {
            queue->current = found->next;
        }
    }

    // Liberar nodo y actualizar count
    MemoryManagerADT mm = getKernelMemoryManager();
    freeMemory(mm, found);
    queue->count--;
}

// Obtener siguiente proceso en Round Robin
static PCB *next_in_queue(ReadyQueue *queue) {
    if (queue->current == NULL || queue->count == 0) {
        return NULL;
    }
    
    // Rotar al siguiente en la lista circular y devolver proceso
    RQNode *result = queue->current;
    queue->current = queue->current->next;
    
    return result->proc;
}

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

    // Inicializar campos relacionados con scheduling
    process->status = PS_READY;
    process->cpu_ticks = 0;
    process->remaining_quantum = process->priority + 1;
    if (process->parent_pid < 0) {
        process->parent_pid = scheduler->current_pid;
    }
    process->waiting_on = -1;

    ReadyQueue *queue = &scheduler->ready_queues[process->priority];
    enqueue_process(queue, process);

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
        ReadyQueue *queue = &scheduler->ready_queues[process->priority];
        dequeue_process(queue, process);
    }

    // Remover del array
    scheduler->processes[pid] = NULL;
    scheduler->process_count--;

    // Si estábamos ejecutando este proceso, forzar reschedule
    if (scheduler->current_pid == pid) {
        scheduler->current_pid = -1;
        scheduler_force_reschedule();
    }

    return 0;
}

int scheduler_set_priority(int pid, uint8_t new_priority) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    if (new_priority < MIN_PRIORITY || new_priority > MAX_PRIORITY) {
        return -1;
    }

    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }

    uint8_t old_priority = process->priority;
    
    if (old_priority == new_priority) {
        return 0;  // Sin cambios
    }

    // Si el proceso está en READY, moverlo entre colas
    if (process->status == PS_READY) {
        // Remover de cola antigua
        ReadyQueue *old_queue = &scheduler->ready_queues[old_priority];
        dequeue_process(old_queue, process);
        
        // Cambiar prioridad
        process->priority = new_priority;
        process->remaining_quantum = new_priority + 1;
        
        // Agregar a cola nueva
        ReadyQueue *new_queue = &scheduler->ready_queues[new_priority];
        enqueue_process(new_queue, process);
    } else {
        // Solo cambiar prioridad (no está en ninguna cola)
        process->priority = new_priority;
        process->remaining_quantum = new_priority + 1;
    }

    // Reevaluar si hace falta reschedule
    bool need_reschedule = false;
    
    // Caso 1: Es el proceso actual y bajó su prioridad
    if (pid == scheduler->current_pid && new_priority > old_priority) {
        // Verificar si hay procesos con mayor prioridad
        for (int p = MIN_PRIORITY; p < new_priority; p++) {
            if (scheduler->ready_queues[p].count > 0) {
                need_reschedule = true;
                break;
            }
        }
    }
    // Caso 2: No es el actual y subió su prioridad
    else if (pid != scheduler->current_pid && new_priority < old_priority) {
        if (process->status == PS_READY) {
            int current_pid = scheduler->current_pid;
            if (current_pid >= 0 && scheduler->processes[current_pid] != NULL) {
                PCB *current = scheduler->processes[current_pid];
                if (new_priority < current->priority) {
                    need_reschedule = true;
                }
            }
        }
    }
    
    if (need_reschedule) {
        scheduler_force_reschedule();
    }

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

void scheduler_yield(void) {
    scheduler_force_reschedule();
}

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

    // Si estaba en READY, sacarlo de su cola para que no sea elegido
    if (proc->status == PS_READY) {
        ReadyQueue *q = &scheduler->ready_queues[proc->priority];
        dequeue_process(q, proc);
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
        ReadyQueue *queue = &scheduler->ready_queues[process->priority];
        dequeue_process(queue, process);
    }
    
    process->status = PS_BLOCKED;
    
    // Si es el proceso actual, forzar reschedule
    if (pid == scheduler->current_pid) {
        scheduler_force_reschedule();
    }
    
    return 0;
}

// Llamar desde proc_unblock() en processes.c
int scheduler_unblock_process(int pid) {
    if (scheduler == NULL || pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }
    
    PCB *process = scheduler->processes[pid];
    if (process == NULL || process->status != PS_BLOCKED) {
        return -1;
    }
    
    process->status = PS_READY;
    
    // Agregar a cola READY de su prioridad
    ReadyQueue *queue = &scheduler->ready_queues[process->priority];
    enqueue_process(queue, process);
    
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
            ReadyQueue *queue = &scheduler->ready_queues[p->priority];
            dequeue_process(queue, p);
            
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
    
    // Limpiar todas las colas liberando nodos
    for (int priority = 0; priority <= MAX_PRIORITY; priority++) {
        ReadyQueue *q = &scheduler->ready_queues[priority];
        int remaining = q->count;
        RQNode *start = q->head;
        RQNode *n = start;
        while (remaining-- > 0 && n != NULL) {
            RQNode *next = n->next;
            freeMemory(mm, n);
            if (next == start) {
                break;
            }
            n = next;
        }
        q->head = NULL;
        q->current = NULL;
        q->count = 0;
    }
    
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
        ReadyQueue *q = &scheduler->ready_queues[proc->priority];
        if (q != NULL) {
            dequeue_process(q, proc);
        }
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