#include "scheduler.h"
#include "process.h" 
#include "lib.h"
#include "queue.h"
#include "queue_array.h"
#include "pipes.h"
#include "interrupts.h" // lo incluí para usar _hlt()
#include "videoDriver.h"
#include <stddef.h>
#include "synchro.h"

extern void timer_tick();

#define SHELL_ADDRESS ((void *) 0x400000)      // TODO: Esto ver si lo movemos a otro archivo (tipo memoryMap.h)

// Prioridad MIN=0 tiene 1 slot, prioridad k tiene k+1 slots
#define SLOTS_FOR_PRIORITY(p) ((p) - MIN_PRIORITY + 1)

// Validación de PID
#define PID_IS_VALID(pid) ((pid) >= 0 && (pid) <= MAX_PID)

// ============================================
//         ESTADO DEL SCHEDULER
// ============================================

static PCB *processes[MAX_PROCESSES];       // Array para acceso por PID
//static queue_t ready_queue = NULL;                 // Cola global de PIDs listos para correr
static queue_t ready_queues_array[NUM_PRIORITIES] ={ NULL } ;          // Array de colas de PIDs listos para correr
static int current_priority = MAX_PRIORITY; // Índice de la cola de prioridad actual
static int remaining_iterations = MAX_PRIORITY;      // Iteraciones restantes en la cola actual
static pid_t current_pid = NO_PID;                // PID del proceso actual
static uint8_t process_count = 0;               // Cantidad total de procesos
static uint64_t total_cpu_ticks = 0;            // Total de ticks de CPU
static bool scheduler_initialized = false;
static pid_t foreground_process_pid = NO_PID;                  // PID  del proceso que está corriendo en foreground

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static PCB *pick_next_process(void);
static void reparent_children_to_init(pid_t pid);
static int init(int argc, char **argv);
static int scheduler_add_init();
static void cleanup_all_processes(void);
static int create_shell();
static void cleanup_resources(PCB * p);
static void reset_scheduler_state(void);

static void cleanup_resources(PCB * p) {
    while (!q_is_empty(p->open_fds)) {
        int fd = q_poll(p->open_fds);
        close_fd(fd);
    }
}

// Proceso init: arranca la shell y se queda haciendo halt para no consumir CPU. Se lo elige siempre que no haya otro proceso para correr!!!!
static int init(int argc, char **argv) {

    if (create_shell() != 0) {
        return -1;
    }

    scheduler_set_foreground_process(SHELL_PID);

    while (1) {
		_hlt();
	}
    return 0;
}

// Devuelve el PID del proceso en foreground o NO_PID si no está inicializado
pid_t scheduler_get_foreground_pid(void) {
    if (!scheduler_initialized) {
        return NO_PID;
    }
    return foreground_process_pid;
}

// Establece el PID del proceso en foreground.
// Devuelve 0 en éxito, -1 en error (scheduler no inicializado o pid inválido).
int scheduler_set_foreground_process(pid_t pid) {
    if (!scheduler_initialized) {
        return -1;
    }

    // Permitimos limpiar el foreground pasando NO_PID
    if (pid != NO_PID && !PID_IS_VALID(pid)) {
        return -1;
    }

    foreground_process_pid = pid;
    return 0;
}



// ============================================
//           INICIALIZACIÓN
// ============================================

static int create_shell(){
    PCB *pcb_shell = proc_create(SHELL_PID, (process_entry_t) SHELL_ADDRESS, 0, NULL, "shell", false, NULL);
    if (pcb_shell == NULL) {
        return -1;
    }
    pcb_shell->priority = MAX_PRIORITY;
    pcb_shell->status = PS_READY; 
    pcb_shell->cpu_ticks = 0;
    processes[SHELL_PID] = pcb_shell;
    process_count++;
    
    // Agregar a la cola de prioridades
    if (!queue_array_add(ready_queues_array, NUM_PRIORITIES, pcb_shell->priority, SHELL_PID)) {
        processes[SHELL_PID] = NULL;
        process_count--;
        free_process_resources(pcb_shell);
        return -1;
    }
    return 0;
}

// Agrega el proceso al array de procesos y a la cola READY
static int scheduler_add_init() {
    if (process_count != 0) { // si no es el primer proceso en ser creado está mal
        return -1;
    }

    PCB *pcb_init = proc_create(INIT_PID, (process_entry_t) init, 0, NULL, "init", false, NULL);
    if (pcb_init == NULL) {
        return -1;
    }

    pcb_init->priority = MIN_PRIORITY;
    pcb_init->status = PS_READY; 
    pcb_init->cpu_ticks = 0;

    processes[INIT_PID] = pcb_init;
    process_count++;

    //timer_tick();

    return 0;
}

int init_scheduler(void) {
    if (scheduler_initialized) { // para no crearlo más de una vez
        return 0;
    }

    // Inicializar array de procesos
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i] = NULL;
    }
    
    // Inicializar array de colas de procesos READY por prioridad
    if (!queue_array_init(ready_queues_array, NUM_PRIORITIES)) {
        return -1;
    }
   
    process_count = 0;
    total_cpu_ticks = 0;
    current_pid = NO_PID; // NO le pongo INIT_PID porque el que lo tiene que elegir es el schedule la primera vez que se llama. 
    //Sino, la primera llamada a scheudule va a tratar a init como current y va a pisar su stack_pointer con prev_rsp

    if(scheduler_add_init() != 0) {
        queue_array_destroy(ready_queues_array, NUM_PRIORITIES);
        return -1;
    }

    scheduler_initialized = true;
    return 0;
}

// ============================================
//        SCHEDULER PRINCIPAL
// ============================================

// ============================================
//    SCHEDULER (índice == prioridad)
// ============================================

void *schedule(void *prev_rsp) {
    if (!scheduler_initialized){
        return prev_rsp;
    } 

    PCB *current = (PID_IS_VALID(current_pid)) ? processes[current_pid] : NULL;

    if (current) {
        current->stack_pointer = prev_rsp;
        current->cpu_ticks++;
        total_cpu_ticks++;

        if (current->status == PS_RUNNING) { 
            current->status = PS_READY;
        }

        if (current->status == PS_READY && current->pid != INIT_PID) {
            // índice == prioridad (1..MAX_PRIORITY)
            if (!queue_array_add(ready_queues_array, NUM_PRIORITIES, current->priority, current->pid)) {
                current->status = PS_RUNNING;
                return prev_rsp;
            }
        }
    }

    PCB *next = pick_next_process();
    if (!next) {
        next = processes[INIT_PID];
    }

    current_pid = next->pid;
    next->status = PS_RUNNING;
    return next->stack_pointer;
}


static inline void advance_priority_window(void) {
    if (--current_priority < MIN_PRIORITY) {
        current_priority = MAX_PRIORITY;
    }
    remaining_iterations = SLOTS_FOR_PRIORITY(current_priority);
}

static PCB *pick_next_process(void) {
    if (!scheduler_initialized || process_count == 0) {
        return NULL;
    }

    int visited_levels = 0;

    while (visited_levels < NUM_PRIORITIES) {
        if (remaining_iterations <= 0) {
            advance_priority_window();
            visited_levels++;
            continue;
        }

        if (queue_array_is_empty(ready_queues_array, NUM_PRIORITIES, current_priority)) {
            advance_priority_window();
            visited_levels++;
            continue;
        }

        pid_t pid = queue_array_poll(ready_queues_array, NUM_PRIORITIES, current_priority);
        if (!PID_IS_VALID(pid)) {
            continue;
        }

        PCB *candidate = processes[pid];
        if (candidate != NULL && candidate->status == PS_READY) {
            if (--remaining_iterations <= 0) {
                advance_priority_window();
            }
            return candidate;
        }
    }
    return NULL;
}

// ============================================
//        GESTIÓN DE PROCESOS
// ============================================

// Agrega el proceso al array de procesos y a la cola READY
int scheduler_add_process(process_entry_t entry, int argc, const char **argv, const char *name, int fds[2]) {
    if (!scheduler_initialized || process_count >= MAX_PROCESSES) {
        return -1;
    }

    // Buscar primer PID libre
    pid_t pid = NO_PID;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] == NULL) {
            pid = i;
            break;
        }
    }
    if (!PID_IS_VALID(pid)) {
        return -1;
    }

    PCB *process = proc_create(pid, entry, argc, argv, name, true, fds);
    if (process == NULL) {
        return -1;
    }

    // Inicializar campos relacionados con scheduling
    process->priority = DEFAULT_PRIORITY; // Asignar prioridad por defecto
    process->status = PS_READY; 
    process->cpu_ticks = 0;

    processes[pid] = process;
    process_count++;

    // Agregar a la cola de prioridades
    if (!queue_array_add(ready_queues_array, NUM_PRIORITIES, process->priority, pid)) {
        processes[pid] = NULL;
        process_count--;
        free_process_resources(process);
        return -1;
    }

    return pid;
}

int scheduler_remove_process(pid_t pid) {
    if (!scheduler_initialized || !PID_IS_VALID(pid)) {
        return -1;
    }

    PCB *process = processes[pid];
    if (!process) {
        return -1;
    }

    // Remover de la cola de procesos listos para correr
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        queue_array_remove(ready_queues_array, NUM_PRIORITIES, process->priority, process->pid);
    }

    // Remover del array
    processes[pid] = NULL;
    process_count--;

    // Si estábamos ejecutando este proceso, forzar reschedule
    if (current_pid == pid) {
        current_pid = -1;
        scheduler_force_reschedule(); 
    }

    // Liberar recursos del proceso
    free_process_resources(process);

    return 0;
}

int scheduler_set_priority(pid_t pid, uint8_t new_priority) {
   if (!scheduler_initialized || !PID_IS_VALID(pid) || 
        processes[pid] == NULL || 
        new_priority < MIN_PRIORITY || new_priority > MAX_PRIORITY) {
        return -1;
    }

    PCB *process = processes[pid];
    int current_priority_level = process->priority;

    if (current_priority_level == new_priority) {
        return 0;
    }

    bool was_ready = (process->status == PS_READY);
    if (was_ready) {
        queue_array_remove(ready_queues_array, NUM_PRIORITIES, current_priority_level, pid);
    }

    process->priority = new_priority;

    if (was_ready) {
        if (!queue_array_add(ready_queues_array, NUM_PRIORITIES, new_priority, pid)) {
            // rollback to previous priority if enqueue fails
            process->priority = current_priority_level;
            queue_array_add(ready_queues_array, NUM_PRIORITIES, current_priority_level, pid);
            return -1;
        }
    }

    return 0;
}

int scheduler_get_priority(pid_t pid) {
    if (!scheduler_initialized || !PID_IS_VALID(pid) || processes[pid] == NULL) {
        return -1;
    }
    PCB *process = processes[pid];
    return process->priority;
}

void scheduler_force_reschedule(void) {
    if (scheduler_initialized) {
        timer_tick(); // llama de nuevo a schedule
    }

}

int scheduler_get_current_pid(void) {
    if (scheduler_initialized) {
        return current_pid;
    }
    return -1;
}


void scheduler_yield(void) {
    scheduler_force_reschedule(); 
}

int scheduler_kill_process(pid_t pid) {
    if(!scheduler_initialized) {
        return -1; // Scheduler not initialized
    }
    
    if (!PID_IS_VALID(pid)) {
        return -2; // Invalid PID
    }
    
    PCB *killed_process = processes[pid];
    if (!killed_process) {
        return -3; // No process found with this PID
    }

    if (!killed_process->killable) {
        return -4; // Process is protected (init or shell)
    }

    reparent_children_to_init(killed_process->pid);

    remove_process_from_all_semaphore_queues(killed_process->pid);

    if (pid == foreground_process_pid) {
        foreground_process_pid = SHELL_PID;
        pipe_on_process_killed(killed_process->pid); // matamos el foreground group si estaba pipeado
    }

    killed_process->status = PS_TERMINATED;
    killed_process->return_value = KILLED_RET_VALUE;
    
    // cierra los fds abiertos
    cleanup_resources(killed_process);


    if(killed_process->parent_pid == INIT_PID) {
        scheduler_remove_process(killed_process->pid); 
    } else { // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (killed_process->status == PS_READY || killed_process->status == PS_RUNNING) {
            queue_array_remove(ready_queues_array, NUM_PRIORITIES, killed_process->priority, killed_process->pid);
        }
        killed_process->status = PS_TERMINATED; // Le cambio el estado despues de hacer el dequeue o sino no va a entrar en la condición del if
        killed_process->return_value = KILLED_RET_VALUE;
        PCB* parent = processes[killed_process->parent_pid];

        if (parent != NULL && parent->status == PS_BLOCKED && parent->waiting_on == killed_process->pid) {
            scheduler_unblock_process(parent->pid);
        }
    }
    if(pid == current_pid){
        scheduler_yield();
    }
    return 0;
}


// ============================================
//   FUNCIONES AUXILIARES PARA BLOQUEO
// ============================================

// Llamar desde proc_block() en processes.c
int scheduler_block_process(pid_t pid) {
    if (!scheduler_initialized || !PID_IS_VALID(pid)) {
        return -1;
    }
    
    PCB *process = processes[pid];
    if (process == NULL) {
        return -1;
    }
    
    // Remover de cola READY (si está ahí)
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        queue_array_remove(ready_queues_array, NUM_PRIORITIES, process->priority, process->pid);
    }
    
    process->status = PS_BLOCKED;
    
    // Si es el proceso actual, forzar reschedule
    if (pid == current_pid) {
        scheduler_force_reschedule();
    }
    
    return 0;
}


int scheduler_unblock_process(pid_t pid) {
    if (!scheduler_initialized || !PID_IS_VALID(pid)) {
        return -1;
    }
    
    PCB *process = processes[pid];
    if (!process || process->status != PS_BLOCKED) {
        return -1;
    }
    
    process->status = PS_READY;
    
    // Agregar a la cola READY según prioridad
    if (!queue_array_add(ready_queues_array, NUM_PRIORITIES, process->priority, pid)) {
        process->status = PS_BLOCKED;
        return -1;
    }

     // --- Wake-up preempt: si el desbloqueado es más prioritario, forzar cambio
    PCB *running = PID_IS_VALID(current_pid) ? processes[current_pid] : NULL;

    if (running && process->priority > running->priority) {
        // para que elija ya esta prioridad
        current_priority = process->priority;
        remaining_iterations = SLOTS_FOR_PRIORITY(current_priority);
        scheduler_force_reschedule();  
    }
    
    return 0;
}

static void cleanup_all_processes(void) {
    if (!scheduler_initialized) {
        return;
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB *p = processes[i];
        if (p) {
            free_process_resources(p);
            processes[i] = NULL;
        }
    }
}

static void reset_scheduler_state(void) {
    process_count = 0;
    total_cpu_ticks = 0;
    current_pid = NO_PID;
    scheduler_initialized = false;
}

void scheduler_destroy(void) {
    if (!scheduler_initialized) {
        return;
    }

    // Limpiar el array de colas por prioridad
    queue_array_destroy(ready_queues_array, NUM_PRIORITIES);

    cleanup_all_processes();

    reset_scheduler_state();
}

// En scheduler.c
PCB *scheduler_get_process(pid_t pid) {
    if (!scheduler_initialized || !PID_IS_VALID(pid)) {
        return NULL;
    }
    
    return processes[pid];
}


int scheduler_get_processes(process_info_t *buffer, int max_count) {
    if (!scheduler_initialized || buffer == NULL || max_count <= 0) {
        return -1;
    }

    int count = 0;
    for (int i = 0; i < MAX_PROCESSES && count < max_count; i++) {
        PCB *p = processes[i];
        Mirando el código de scheduler_get_processes():
        
        La respuesta corta: NO se bloquea.
        
        Razones:
        
        ✅ No hay locks/mutexes - el código accede directamente a processes[] sin sincronización
        ✅ No hay syscalls bloqueantes - solo lee memoria local
        ✅ No hay loops infinitos - itera MAX_PROCESSES veces como máximo
        ✅ No hay calls a funciones bloqueantes - strncpy() es rápido
        El ÚNICO problema potencial: Race condition, NO bloqueo
        Escenario:
        
        
        if (p) {
            buffer[count].pid = p->pid;
            strncpy(buffer[count].name, p->name, MAX_PROCESS_NAME_LENGTH);
            buffer[count].status = p->status;
            buffer[count].priority = p->priority;
            buffer[count].parent_pid = p->parent_pid;
            buffer[count].read_fd = p->read_fd;
            buffer[count].write_fd = p->write_fd;
            buffer[count].stack_base = (uint64_t)p->stack_base;
            buffer[count].stack_pointer = (uint64_t)p->stack_pointer;
            
            count++;
        }
    }

    return count;
}

// ============================================
//   TERMINACIÓN INMEDIATA DEL PROCESO ACTUAL
// ============================================

void scheduler_exit_process(int64_t ret_value) {
    if(!scheduler_initialized) {
        return;
    }
    
    PCB * current_process = processes[current_pid];
    reparent_children_to_init(current_process->pid);

    if (current_pid == foreground_process_pid) {
        foreground_process_pid = SHELL_PID;
    }
    remove_process_from_all_semaphore_queues(current_process->pid);
    
    // limpia los fds abiertos
    cleanup_resources(current_process);

    if(current_process->parent_pid == INIT_PID) { // si el padre es init, no hace falta mantener su pcb para guardarnos ret_value pues nadie le va a hacer waitpid
        scheduler_remove_process(current_process->pid); 
    } else{ // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (current_process->status == PS_READY || current_process->status == PS_RUNNING) {
            queue_array_remove(ready_queues_array, NUM_PRIORITIES, current_process->priority, current_process->pid);
        }
        current_process->status = PS_TERMINATED; // Le cambio el estado despues de hacer el dequeue o sino no va a entrar en la condición del if
        current_process->return_value = ret_value;
        PCB* parent = processes[current_process->parent_pid];

        // Si el padre estaba bloqueado haciendo waitpid, lo desbloqueamos
        if (parent->status == PS_BLOCKED && parent->waiting_on == current_process->pid) {
            scheduler_unblock_process(parent->pid);
        }
    
    }
    scheduler_yield();
}


// Bloquea al proceso actual hasta que el hijo indicado termine. Devuelve el status del hijo si ya terminó o 0.
int scheduler_waitpid(pid_t child_pid) {
    if (!scheduler_initialized || !PID_IS_VALID(child_pid) || 
        processes[child_pid] == NULL || 
        processes[child_pid]->parent_pid != current_pid) {
        return -1;
    }
    
    // Si el proceso hijo no termino, bloqueamos el proceso actual hasta que termine
    if (processes[child_pid]->status != PS_TERMINATED) {
        processes[current_pid]->waiting_on = child_pid;
        scheduler_block_process(current_pid);
    }

    // Llega acá cuando el hijo terminó y lo desbloqueo o si el hijo ya había terminado
    
    processes[current_pid]->waiting_on = NO_PID;
    int ret_value = processes[child_pid]->return_value;
    scheduler_remove_process(child_pid);

    return ret_value;
}

int adopt_init_as_parent(pid_t pid) {
    if(!scheduler_initialized || !PID_IS_VALID(pid)) {
        return -1;
    }
    PCB *init_process = processes[INIT_PID];
    if (init_process == NULL) {
        return -1;
    }

    PCB *orphan_process = processes[pid];
    if (orphan_process == NULL) {
        return -1;
    }

    orphan_process->parent_pid = INIT_PID;
    return 0;
}

static void reparent_children_to_init(pid_t pid) {
    if(!scheduler_initialized || !PID_IS_VALID(pid)) {
        return;
    }
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (processes[i] != NULL &&
			processes[i]->parent_pid == pid) {
            // Asignar el proceso huérfano al init
            if(processes[i]->status == PS_TERMINATED){
                // Si el hijo ya estaba terminado, lo removemos directamente
                scheduler_remove_process(processes[i]->pid);
            } else{
                processes[i]->parent_pid = INIT_PID;
            }   
        }
	}
}


// Mata el proceso que está en foreground. Devuelve 0 en éxito, -1 en error.
int scheduler_kill_foreground_process(void) {
    if (!scheduler_initialized) {
        return -1;
    }

    // Si no hay proceso en foreground, retornar error
    if (foreground_process_pid == NO_PID) {
        return -1;
    }

    int result = scheduler_kill_process(foreground_process_pid);
    // Después de matar el proceso en foreground, establecer la shell como proceso en foreground
    foreground_process_pid = SHELL_PID;
    
    return result;
}
