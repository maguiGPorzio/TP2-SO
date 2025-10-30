#include "scheduler.h"
#include "process.h" 
#include <memoryManager.h>
#include "lib.h"
#include "queue.h"
#include "interrupts.h" // lo incluí para usar _hlt()
#include "videoDriver.h"

//extern void timer_tick();

#define SHELL_ADDRESS ((void *) 0x400000)      // TODO: Esto ver si lo movemos a otro archivo (tipo memoryMap.h)


// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================


typedef struct SchedulerCDT {
    PCB *processes[MAX_PROCESSES];              // Array para acceso por PID
    queue_t ready_queue;                        // Cola de PIDs listos para correr
    int current_pid;                             // PID del proceso actual
    uint8_t process_count;                       // Cantidad total de procesos
    uint64_t total_cpu_ticks;                    // Total de ticks de CPU
    bool force_reschedule;                       // Flag para forzar cambio de proceso: la próxima vez que se llame a schedule, elegir otro proceso aunqur no se haya acabado el quantum del actual (si se bloquea, termina, se le hizo kill o hace yield)
} SchedulerCDT;

// ============================================
//         VARIABLES GLOBALES
// ============================================

static SchedulerADT scheduler = NULL;

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static PCB *pick_next_process(void);
static void reparent_children_to_init(int16_t pid);
static int init(int argc, char **argv);
static int scheduler_add_init();
static inline bool pid_is_valid(pid_t pid);
static void cleanup_all_processes(void);

static inline bool pid_is_valid(pid_t pid) {
    return pid >= 0 && pid <= MAX_PID;
}

// Proceso init: arranca la shell y se queda haciendo halt para no consumir CPU. Se lo elige siempre que no haya otro proceso para correr!!!!
static int init(int argc, char **argv) {
    const char **shell_args = { NULL };

    int shell_pid = scheduler_add_process((process_entry_t) SHELL_ADDRESS, 0, shell_args, "shell");
    scheduler_set_priority(shell_pid, MAX_PRIORITY);

    while (1) {
		_hlt();
	}
    return 0;
}

// ============================================
//           INICIALIZACIÓN
// ============================================

// Agrega el proceso al array de procesos y a la cola READY
static int scheduler_add_init() {
    if (!scheduler || scheduler->process_count != 0) { // si no es el primer proceso en ser creado está mal
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

    //timer_tick();

    return 0;
}

SchedulerADT init_scheduler(void) {
    if (scheduler != NULL) { // para no crearlo más de una vez
        return scheduler;
    }

    MemoryManagerADT mm = get_kernel_memory_manager();
    scheduler = alloc_memory(mm, sizeof(SchedulerCDT));
    
    if (scheduler == NULL) {
        return NULL;
    }

    // Inicializar array de procesos
    for (int i = 0; i < MAX_PROCESSES; i++) {
        scheduler->processes[i] = NULL;
    }
    
    // Inicializar cola de procesos READY
    scheduler->ready_queue = q_init();
    if (scheduler->ready_queue == NULL) {
        free_memory(mm, scheduler);
        scheduler = NULL;
        return NULL;
    }
   
    scheduler->process_count = 0;
    scheduler->total_cpu_ticks = 0;
    scheduler->force_reschedule = false;
    scheduler->current_pid = NO_PID; // NO le pongo INIT_PID porque el que lo tiene que elegir es el schedule la primera vez que se llama. 
    //Sino, la primera llamada a scheudule va a tratar a init como current y va a pisar su stack_pointer con prev_rsp

    if(scheduler_add_init() != 0) {
        q_destroy(scheduler->ready_queue);
        free_memory(mm, scheduler);
        scheduler = NULL;
        return NULL;
    }

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

    PCB *current = (pid_is_valid(scheduler->current_pid)) ? scheduler->processes[scheduler->current_pid] : NULL;

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
        if (current->status == PS_READY && current->pid != INIT_PID) {
            if (scheduler->ready_queue == NULL || !q_add(scheduler->ready_queue, current->pid)) {
                current->status = PS_RUNNING;
                current->remaining_quantum = current->priority;
                scheduler->force_reschedule = false;
                return prev_rsp;
            }
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

    if (scheduler->ready_queue == NULL || q_is_empty(scheduler->ready_queue)) {
        return NULL;
    }

    while (!q_is_empty(scheduler->ready_queue)) {
        int next_pid = q_poll(scheduler->ready_queue);
        if (!pid_is_valid(next_pid)) {
            continue;
        }
        PCB *candidate = scheduler->processes[next_pid];
        if (candidate != NULL && candidate->status == PS_READY) {
            return candidate;
        }
        // Si el proceso ya no está listo, seguimos buscando
    }

    return NULL;
}


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
    if (!pid_is_valid(pid)) {
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

    if (scheduler->ready_queue == NULL || !q_add(scheduler->ready_queue, pid)) {
        scheduler->processes[pid] = NULL;
        scheduler->process_count--;
        free_process_resources(process);
        return -1;
    }

    return pid;
}

int scheduler_remove_process(int pid) {
    if (!scheduler || !pid_is_valid(pid)) {
        return -1;
    }

    PCB *process = scheduler->processes[pid];
    if (!process) {
        return -1;
    }

    // Remover de la cola de procesos listos para correr
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        if (scheduler->ready_queue != NULL) {
            q_remove(scheduler->ready_queue, process->pid);
        }
    }

    // Remover del array
    scheduler->processes[pid] = NULL;
    scheduler->process_count--;

    // Si estábamos ejecutando este proceso, forzar reschedule
    if (scheduler->current_pid == pid) {
        scheduler->current_pid = -1;
        scheduler_force_reschedule(); 
    }

    // Liberar recursos del proceso
    free_process_resources(process);

    return 0;
}

int scheduler_set_priority(int pid, uint8_t new_priority) {
   if (scheduler == NULL || !pid_is_valid(pid) || 
        scheduler->processes[pid] == NULL || 
        new_priority < MIN_PRIORITY || new_priority > MAX_PRIORITY) {
        return -1;
    }

    scheduler->processes[pid]->priority = new_priority;
    scheduler->processes[pid]->remaining_quantum = new_priority;
    return 0;
}

int scheduler_get_priority(int pid) {
    if (scheduler == NULL || !pid_is_valid(pid) || scheduler->processes[pid] == NULL) {
        return -1;
    }
    PCB *process = scheduler->processes[pid];
    return process->priority;
}

void scheduler_force_reschedule(void) {
    if (scheduler) {
        scheduler->force_reschedule = true;
        timer_tick();
    }

}

int scheduler_get_current_pid(void) {
    if (scheduler) {
        return scheduler->current_pid;
    }
    return -1;
}


void scheduler_yield(void) {
    scheduler_force_reschedule(); 
}

int scheduler_kill_process(int pid) {
    if(!scheduler || !pid_is_valid(pid)) {
        return -1;
    }
    
    PCB *killed_process = scheduler->processes[pid];
    if (!killed_process) {
        return -1;
    }
    reparent_children_to_init(killed_process->pid);

    if(killed_process->parent_pid == INIT_PID) { // si el padre es init, no hace falta mantener su pcb para guardarnos ret_value pues nadie le va a hacer waitpid
        scheduler_remove_process(killed_process->pid); 
    } else{ // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (killed_process->status == PS_READY || killed_process->status == PS_RUNNING) {
            if (scheduler->ready_queue != NULL) {
                q_remove(scheduler->ready_queue, killed_process->pid);
            }
        }
        killed_process->status = PS_TERMINATED; // Le cambio el estado despues de hacer el dequeue o sino no va a entrar en la condición del if
        killed_process->return_value = KILLED_RET_VALUE;
        PCB* parent = scheduler->processes[killed_process->parent_pid];

        // Si el padre estaba bloqueado haciendo waitpid, lo desbloqueamos
        if (parent->status == PS_BLOCKED && parent->waiting_on == killed_process->pid) {
            scheduler_unblock_process(parent->pid);
        }
    
    }
    if(pid == scheduler->current_pid){
        scheduler_yield();
    }
    return 0;
}


// ============================================
//   FUNCIONES AUXILIARES PARA BLOQUEO
// ============================================

// Llamar desde proc_block() en processes.c
int scheduler_block_process(int pid) {
    if (!scheduler || !pid_is_valid(pid)) {
        return -1;
    }
    
    PCB *process = scheduler->processes[pid];
    if (process == NULL) {
        return -1;
    }
    
    // Remover de cola READY (si está ahí)
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        if (scheduler->ready_queue != NULL) {
            q_remove(scheduler->ready_queue, process->pid);
        }
    }
    
    process->status = PS_BLOCKED;
    
    // Si es el proceso actual, forzar reschedule
    if (pid == scheduler->current_pid) {
        scheduler_force_reschedule();
    }
    
    return 0;
}


int scheduler_unblock_process(int pid) {
    if (!scheduler || !pid_is_valid(pid)) {
        return -1;
    }
    
    PCB *process = scheduler->processes[pid];
    if (!process || process->status != PS_BLOCKED) {
        return -1;
    }
    
    process->status = PS_READY;
    
    // Agregar a la cola READY global
    if (scheduler->ready_queue == NULL || !q_add(scheduler->ready_queue, pid)) {
        process->status = PS_BLOCKED;
        return -1;
    }
    
    return 0;
}

static void cleanup_all_processes(void) {
    if (!scheduler) {
        return;
    }

    // MemoryManagerADT mm = get_kernel_memory_manager();

    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB *p = scheduler->processes[i];
        if (p) {
            free_process_resources(p);
        }
    }
}

void scheduler_destroy(void) {
    if (!scheduler) {
        return;
    }

    MemoryManagerADT mm = get_kernel_memory_manager();

    // Limpiar la cola global liberando nodos
    q_destroy(scheduler->ready_queue);
    scheduler->ready_queue = NULL;

    cleanup_all_processes();

    free_memory(mm, scheduler);
    scheduler = NULL;
}

// En scheduler.c
PCB *scheduler_get_process(int pid) {
    if (!scheduler || !pid_is_valid(pid)) {
        return NULL;
    }
    
    return scheduler->processes[pid];
}

void scheduler_print_processes() {
    if (!scheduler) {
        return;
    }

    uint32_t color = 0xFFFFFF;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB * p = scheduler->processes[i];
        if (p) {
            vdPrint(p->name, color);
            vdPrint("   STATUS: ", color);
            switch (p->status) {
                case PS_READY:      vdPrint("READY", color);      break;
                case PS_RUNNING:    vdPrint("RUNNING", color);    break;
                case PS_BLOCKED: vdPrint("BLOCKED", color); break;
                case PS_TERMINATED: vdPrint("TERMINATED", color); break;
                default:            vdPrint("UNKNOWN", color);    break;
            }
            vdPrint("\n", color);
        }
    }
}

// ============================================
//   TERMINACIÓN INMEDIATA DEL PROCESO ACTUAL
// ============================================

void scheduler_exit_process(int64_t ret_value) {
    if(!scheduler) {
        return;
    }
    
    PCB * current_process = scheduler->processes[scheduler->current_pid];
    reparent_children_to_init(current_process->pid);

    if(current_process->parent_pid == INIT_PID) { // si el padre es init, no hace falta mantener su pcb para guardarnos ret_value pues nadie le va a hacer waitpid
        scheduler_remove_process(current_process->pid); 
    } else{ // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (current_process->status == PS_READY || current_process->status == PS_RUNNING) {
            if (scheduler->ready_queue != NULL) {
                q_remove(scheduler->ready_queue, current_process->pid);
            }
        }
        current_process->status = PS_TERMINATED; // Le cambio el estado despues de hacer el dequeue o sino no va a entrar en la condición del if
        current_process->return_value = ret_value;
        PCB* parent = scheduler->processes[current_process->parent_pid];

        // Si el padre estaba bloqueado haciendo waitpid, lo desbloqueamos
        if (parent->status == PS_BLOCKED && parent->waiting_on == current_process->pid) {
            scheduler_unblock_process(parent->pid);
        }
    
    }
    scheduler_yield();
}


// Bloquea al proceso actual hasta que el hijo indicado termine. Devuelve el status del hijo si ya terminó o 0.
int scheduler_waitpid(int child_pid) {
    if (!scheduler || !pid_is_valid(child_pid) || 
        scheduler->processes[child_pid] == NULL || 
        scheduler->processes[child_pid]->parent_pid != scheduler->current_pid) {
        return -1;
    }
    
    // Si el proceso hijo no termino, bloqueamos el proceso actual hasta que termine
    if (scheduler->processes[child_pid]->status != PS_TERMINATED) {
        scheduler->processes[scheduler->current_pid]->waiting_on = child_pid;
        scheduler_block_process(scheduler->current_pid);
    }

    // Llega acá cuando el hijo terminó y lo desbloqueo o si el hijo ya había terminado
    
    scheduler->processes[scheduler->current_pid]->waiting_on = NO_PID;
    int ret_value = scheduler->processes[child_pid]->return_value;
    scheduler_remove_process(child_pid);

    return ret_value;
}

static void reparent_children_to_init(int16_t pid) {
    if(!scheduler || !pid_is_valid(pid)) {
        return;
    }
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (scheduler->processes[i] != NULL &&
			scheduler->processes[i]->parent_pid == pid) {
            // Asignar el proceso huérfano al init
            if(scheduler->processes[i]->status == PS_TERMINATED){
                // Si el hijo ya estaba terminado, lo removemos directamente
                scheduler_remove_process(scheduler->processes[i]->pid);
            } else{
                scheduler->processes[i]->parent_pid = INIT_PID;
            }   
        }
			
	}
}
