#include "scheduler.h"
#include "process.h" 
#include <memoryManager.h>
#include "lib.h"

// Variables compartidas con ASM para solicitar cambio de contexto
// ASM escribe current_kernel_rsp al entrar a la syscall (después de pushState)
// C escribe switch_to_rsp cuando quiere que el handler cambie a otra pila antes de iretq
void *current_kernel_rsp = 0;
void *switch_to_rsp = 0;
extern void *syscall_frame_ptr; // RSP de la shell capturado antes de cambiar a un proceso

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================

// Cola circular de procesos READY (una por prioridad)
typedef struct ReadyQueue {
    PCB *head;          // Primer proceso en la cola
    PCB *current;       // Proceso actual en rotación Round Robin
    int count;          // Cantidad de procesos en esta cola
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
//    (usando Lista Circular - O(10) vs O(N))
// ============================================

static PCB *pick_next_process(void) {
    if (scheduler == NULL || scheduler->process_count == 0) {
        return NULL;
    }

    // Si el proceso actual aún tiene quantum Y no se forzó reschedule
    if (!scheduler->force_reschedule && scheduler->current_pid >= 0) {
        PCB *current = scheduler->processes[scheduler->current_pid];
        
        if (current != NULL && 
            current->status == PS_READY && 
            current->remaining_quantum > 0) {
            return current;
        }
    }

    // Buscar proceso con mayor prioridad (menor número)
    // Recorrer las colas de prioridad de 0 a 9
    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        ReadyQueue *queue = &scheduler->ready_queues[priority];
        
        if (queue->count > 0) {
            // Hay procesos en esta cola de prioridad
            // Round Robin: rotar al siguiente
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

    if (queue->head == NULL) {
        // Primera inserción: lista circular de 1 elemento
        queue->head = process;
        process->next = process;
        process->prev = process;
        queue->current = process;
    } else {
        // Insertar al final (antes del head)
        PCB *tail = queue->head->prev;
        
        tail->next = process;
        process->prev = tail;
        process->next = queue->head;
        queue->head->prev = process;
    }
    
    queue->count++;
}

// Remover proceso de la cola circular
static void dequeue_process(ReadyQueue *queue, PCB *process) {
    if (process == NULL || queue->count == 0) {
        return;
    }
    
    if (queue->count == 1) {
        // Último elemento
        queue->head = NULL;
        queue->current = NULL;
    } else {
        // Desenlazar de la lista circular
        process->prev->next = process->next;
        process->next->prev = process->prev;
        
        // Ajustar head si es necesario
        if (queue->head == process) {
            queue->head = process->next;
        }
        
        // Ajustar current si es necesario
        if (queue->current == process) {
            queue->current = process->next;
        }
    }
    
    // Limpiar punteros del proceso
    process->next = NULL;
    process->prev = NULL;
    queue->count--;
}

// Obtener siguiente proceso en Round Robin
static PCB *next_in_queue(ReadyQueue *queue) {
    if (queue->current == NULL || queue->count == 0) {
        return NULL;
    }
    
    // Rotar al siguiente en la lista circular
    PCB *result = queue->current;
    queue->current = queue->current->next;
    
    return result;
}

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

int scheduler_add_process(process_entry_t entry, int argc, const char **argv, const char *name) {
    if (scheduler == NULL || scheduler->process_count >= MAX_PROCESSES) {
        return -1;
    }

    PCB *process=proc_spawn(entry, argc, argv, name);

    int pid = process->pid;
    if (pid < 0 || pid >= MAX_PROCESSES) {
        return -1;
    }

    if (scheduler->processes[pid] != NULL) {
        return -1;  // Slot ya ocupado
    }

    // Agregar al array (para acceso por PID)
    scheduler->processes[pid] = process;
    scheduler->process_count++;

    // Inicializar campos relacionados con scheduling
    process->status = PS_READY;
    process->cpu_ticks = 0;
    process->remaining_quantum = process->priority + 1;
    
    // Inicializar punteros de lista circular
    process->next = NULL;
    process->prev = NULL;

    // Agregar a la cola de su prioridad
    ReadyQueue *queue = &scheduler->ready_queues[process->priority];
    enqueue_process(queue, process);

    return 0;
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

void scheduler_get_stats(uint64_t *total_ticks) {
    if (scheduler != NULL && total_ticks != NULL) {
        *total_ticks = scheduler->total_cpu_ticks;
    }
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
            // Remover de cola si está ahí (no debería, pero por seguridad)
            if (p->next != NULL || p->prev != NULL) {
                ReadyQueue *queue = &scheduler->ready_queues[p->priority];
                dequeue_process(queue, p);
            }
            
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
    
    // Limpiar todas las colas
    for (int priority = 0; priority <= MAX_PRIORITY; priority++) {
        scheduler->ready_queues[priority].head = NULL;
        scheduler->ready_queues[priority].current = NULL;
        scheduler->ready_queues[priority].count = 0;
    }
    
    // No liberamos los PCBs aquí, eso lo hace cleanup_all_processes()
    freeMemory(mm, scheduler);
    scheduler = NULL;
}

// ============================================
//        DEBUG / ESTADÍSTICAS
// ============================================

void scheduler_print_queues(void) {
    if (scheduler == NULL) {
        return;
    }
    
    printf("=== Ready Queues ===\n");
    for (int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        ReadyQueue *queue = &scheduler->ready_queues[priority];
        
        if (queue->count > 0) {
            printf("Priority %d (%d processes): ", priority, queue->count);
            
            PCB *p = queue->head;
            for (int i = 0; i < queue->count; i++) {
                printf("%s(PID=%d) ", p->name, p->pid);
                p = p->next;
            }
            printf("\n");
        }
    }
    printf("\n");
}

void proc_yield(void) {
    scheduler_force_reschedule();
}
