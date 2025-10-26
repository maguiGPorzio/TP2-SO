#include "scheduler.h"
#include "process.h" 
#include <memoryManager.h>
#include "lib.h"
#include "ready_queue.h"

#define SHELL_ADDRESS ((void *) 0x400000)      // TODO: Esto ver si lo movemos a otro archivo (tipo memoryMap.h)
#define INIT_PID 0 // PID del proceso init

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================


typedef struct SchedulerCDT {
    PCB *processes[MAX_PROCESSES];              // Array para acceso por PID
    ReadyQueue ready_queue;                      // Cola de procesos READY (round-robin)
    int current_pid;                             // PID del proceso actual
    uint8_t process_count;                       // Cantidad total de procesos
    uint64_t total_cpu_ticks;                    // Total de ticks de CPU
    bool force_reschedule;                       // Flag para forzar cambio de proceso: la próxima vez que se llame a schedule, elegir otro proceso aunqur no se haya acabado el quantum del actual (si se bloquea, termina, se le hizo kill o hace yield)
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


static void cleanup_terminated_orphans(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler->processes[i] != NULL && 
            scheduler->processes[i]->status == PS_TERMINATED && 
            scheduler->processes[i]->parent_pid == 0) {
            scheduler_remove_process(i);
        }
    }
}

// Proceso init: arranca la shell y se queda limpiando procesos huérfanos terminados y haciendo halt para no consumir CPU. Se lo elige siempre que no haya otro proceso para correr!!!!
static int init(int argc, char **argv) {
    char **shell_args = { NULL };

    int shell_pid = scheduler_add_process((process_entry_t) SHELL_ADDRESS, 0, shell_args, "shell");
    scheduler_set_priority(shell_pid, MAX_PRIORITY);

    while (1) {
        cleanup_terminated_orphans();
		_hlt();
	}
    return 0;
}

// ============================================
//           INICIALIZACIÓN
// ============================================

// Agrega el proceso al array de procesos y a la cola READY
static int scheduler_add_init() {
    if (scheduler == NULL || scheduler->process_count != 0) { // si no es el primer proceso en ser creado está mal
        return -1;
    }

    PCB *pcb_init = proc_create(INIT_PID, (process_entry_t) init, 0, NULL, "init");
    if (pcb_init == NULL) {
        return -1;
    }

    pcb_init->priority = MIN_PRIORITY;
    pcb_init->status = PS_READY; 
    pcb_init->cpu_ticks = 0;
    pcb_init->remaining_quantum = pcb_init->priority;

    scheduler->processes[INIT_PID] = pcb_init;
    scheduler->process_count++;

    return 0;
}

SchedulerADT init_scheduler(void) {
    if (scheduler != NULL) { // para no crearlo más de una vez
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
   
    if(scheduler_add_init() != 0) {
        freeMemory(mm, scheduler);
        scheduler = NULL;
        return NULL;
    }

    scheduler->current_pid = INIT_PID;
    scheduler->process_count = 1;
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
    if (!scheduler) {
        return prev_rsp;
    }

    PCB *current = (scheduler->current_pid >= 0) ? scheduler->processes[scheduler->current_pid] : NULL;

    if (current) {
        current->stack_pointer = prev_rsp; // actualiza el rsp del proceso que estuvo corriendo hasta ahora en su pcb
        
        current->cpu_ticks++; // cuantas veces interrumpimos este proceso que estuvo corrriendo hasta ahora
        scheduler->total_cpu_ticks++;

        if (current->status == PS_RUNNING && current->remaining_quantum > 0) { // si se lo interrumpió antes de que se termine su quantum y estaba corriendo
            current->remaining_quantum--;
        }
        
        if(current->status == PS_RUNNING && current->remaining_quantum > 0 && !scheduler->force_reschedule){ // si aún tiene quantum y no se forzó reschedule, seguimos corriendo el mismo
            return prev_rsp;
        }

        if(current->status == PS_RUNNING){ // si no entró en el if anterior, es porque no tiene que seguir corriendo. Si su status es RUNNING, la razón no fue por haberse bloqueado, terminado o porque se le hizo kill, entonces hay que cambiar su status a READY. En el caso de que se lo hubiera bloqueado, matado o terminado, ya su status se cambió en otras funciones
            current->status = PS_READY;
        }
    }
    // Si el proceso actual tiene que cambiar:
    PCB *next = pick_next_process();
    
    // Si no hay otro proceso listo, usar el proceso init como fallback
    if (!next) {
        next = scheduler->processes[INIT_PID];
    }

    scheduler->current_pid = next->pid;
    next->status = PS_RUNNING;
    next->remaining_quantum = next->priority; // reseteo del quantum
    scheduler->force_reschedule = false;
    return next->stack_pointer;

}

// ============================================
//    ALGORITMO: ROUND ROBIN CON PRIORIDADES
// ============================================

// Devuelve null si no hay proceso listo para correr en la cola
static PCB *pick_next_process(void) {
    if (scheduler == NULL || scheduler->process_count == 0) {
        return NULL;
    }
    
    ReadyQueue *queue = &scheduler->ready_queue;
    if (queue->count == 0) {
        return NULL;
    }
    PCB *candidate = ready_queue_next(queue);

    return candidate;
}

// ============================================
//     OPERACIONES DE LISTA CIRCULAR
// ============================================

// Lista circular (enqueue/dequeue/next) movida a ready_queue.c

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

// Agrega el proceso al array de procesos y a la cola READY
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

    // Inicializar campos relacionados con scheduling
    process->priority = DEFAULT_PRIORITY; // Asignar prioridad por defecto
    process->status = PS_READY; 
    process->cpu_ticks = 0;
    process->remaining_quantum = process->priority;


    scheduler->processes[pid] = process;
    scheduler->process_count++;

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
        scheduler_force_reschedule(); 
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


void scheduler_yield(void) {
    scheduler_force_reschedule(); 
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

    if(proc->status == PS_RUNNING){
        scheduler->current_pid = -1;    
        scheduler_force_reschedule();
        ready_queue_dequeue(&scheduler->ready_queue, proc);
    }

    // Si estaba en READY, sacarlo de la cola global para que no sea elegido
    if (proc->status == PS_READY) {
        ready_queue_dequeue(&scheduler->ready_queue, proc);
    }

    // Marcar como TERMINATED (la liberación real la hace cleanup_terminated_processes)
    proc->status = PS_TERMINATED;

    // TODO: hacer que sus hijos sean adoptados por init

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