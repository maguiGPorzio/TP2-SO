// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com


#include "scheduler.h"
#include "process.h" 
#include "lib.h"
#include "queue.h"
#include "pipes.h"
#include "interrupts.h" 
#include "videoDriver.h"
#include "../include/time.h"
#include <stddef.h>

extern void timer_tick();

#define SHELL_ADDRESS ((void *) 0x400000)      // direccion donde empieza userland


// ============================================
//         ESTADO DEL SCHEDULER
// ============================================

static PCB *processes[MAX_PROCESSES];
static queue_t ready_queue[PRIORITY_COUNT] = {0};

static pid_t current_pid = NO_PID;                
static uint8_t process_count = 0;               
static uint64_t total_cpu_ticks = 0;            
static bool force_reschedule = false;           
static bool scheduler_initialized = false;
static pid_t foreground_process_pid = NO_PID;

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static PCB *pick_next_process(void);
static void reparent_children_to_init(pid_t pid);
static int init(int argc, char **argv);
static int scheduler_add_init();
static inline bool pid_is_valid(pid_t pid);
static void cleanup_all_processes(void);
static int create_shell();
static inline bool pid_is_valid(pid_t pid) ;
static void close_open_fds(PCB * p);
static void apply_aging(void);

static inline bool pid_is_valid(pid_t pid) {
    return pid >= 0 && pid <= MAX_PID;
}

static void close_open_fds(PCB * p) {
    while (!q_is_empty(p->open_fds)) {
        int fd = q_poll(p->open_fds);
        close_fd(fd);
    }
}

// Proceso init: arranca la shell y se queda haciendo halt para no consumir CPU. Se lo elige siempre que no haya otro proceso para correr!!!!
// funciona tambien como idle
static int init(int argc, char **argv) {

    if (create_shell() != 0) {
        return -1;
    }

    scheduler_set_foreground_process(SHELL_PID);

    while (1) {
		_hlt();
	}

    return -1;
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
    if (pid != NO_PID && !pid_is_valid(pid)) {
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
    pcb_shell->effective_priority = MAX_PRIORITY;
    pcb_shell->status = PS_READY; 
    pcb_shell->cpu_ticks = 0;
    pcb_shell->last_tick = ticks_elapsed();
    processes[SHELL_PID] = pcb_shell;
    process_count++;
    if (!q_add(ready_queue[pcb_shell->effective_priority], SHELL_PID)) {
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

    pcb_init->priority = MIN_PRIORITY; // no importa pues no va a ninguna queue
    pcb_init->effective_priority = MIN_PRIORITY;
    pcb_init->status = PS_READY; 
    pcb_init->cpu_ticks = 0;
    pcb_init->last_tick = 0; // no importa pues no va a ninguna queue

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
    
    // inicializamos las queues
    for (int i = MAX_PRIORITY; i <= MIN_PRIORITY; i++) {
        ready_queue[i] = q_init();
        if (ready_queue[i] == NULL) {
            return -1;
        }
    }
    
   
    process_count = 0;
    total_cpu_ticks = 0;
    force_reschedule = false;
    current_pid = NO_PID; // NO le pongo INIT_PID porque el que lo tiene que elegir es el schedule la primera vez que se llama. 
    //Sino, la primera llamada a scheudule va a tratar a init como current y va a pisar su stack_pointer con prev_rsp

    if(scheduler_add_init() != 0) {
        for (int i = MAX_PRIORITY; i <= MIN_PRIORITY; i++) {
        q_destroy(ready_queue[i]);
    }

        return -1;
    }

    scheduler_initialized = true;
    return 0;
}

// ============================================
//        SCHEDULER PRINCIPAL
// ============================================

void * schedule(void * prev_rsp) {
    if (!scheduler_initialized) {
        return prev_rsp;
    }

    PCB *current = (pid_is_valid(current_pid)) ? processes[current_pid] : NULL;

    if (current) {
        current->stack_pointer = prev_rsp; // actualiza el rsp del proceso que estuvo corriendo hasta ahora en su pcb
        
        current->cpu_ticks++; // cuantas veces interrumpimos este proceso que estuvo corrriendo hasta ahora
        total_cpu_ticks++;

        // Cuando un proceso deja de correr (por cualquier razón), vuelve a su prioridad base
        // (pierde cualquier promoción temporal por aging)
        current->effective_priority = current->priority;

        // Con quantum de 1 tick, siempre forzamos reschedule después de cada tick
        // (a menos que el proceso ya no esté RUNNING por otra razón)
        
        if(current->status == PS_RUNNING){ 
            // Si el status es RUNNING, cambiar a READY
            // Si fue bloqueado, terminado o matado, el status ya se cambió en otras funciones
            current->status = PS_READY;
        }
        
        if (current->status == PS_READY && current->pid != INIT_PID) {
            // Agregar a la cola correspondiente a su prioridad efectiva (que ya fue reseteada a priority)
            queue_t target_queue = ready_queue[current->effective_priority];
            if (!q_add(target_queue, current->pid)) {
                current->status = PS_RUNNING;
                force_reschedule = false;
                return prev_rsp;
            }
        }
    }

    // Aplicar aging cada N ticks
    if (total_cpu_ticks % AGING_CHECK_INTERVAL == 0) {
        apply_aging();
    }

    // Si el proceso actual tiene que cambiar:
    PCB *next = pick_next_process();
    
    // Si no hay otro proceso listo, usar el proceso init como fallback
    if (!next) {
        next = processes[INIT_PID];
    }

    // Cuando un proceso va a correr:
    // Actualizar su last_tick para el control de aging
    next->last_tick = total_cpu_ticks;

    current_pid = next->pid;
    next->status = PS_RUNNING;
    force_reschedule = false;
    return next->stack_pointer;

}

// ============================================
//    ALGORITMO: MULTICOLA CON PRIORIDADES
// ============================================

// Devuelve null si no hay proceso listo para correr en ninguna cola
// Recorre las colas de mayor prioridad (0) a menor (PRIORITY_COUNT-1)
static PCB *pick_next_process(void) {
    if (!scheduler_initialized || process_count == 0) {
        return NULL;
    }

    // Recorrer las colas por orden de prioridad
    for (int priority = MAX_PRIORITY; priority <= MIN_PRIORITY; priority++) {
        if (q_is_empty(ready_queue[priority])) {
            continue;
        }
        
        // Buscar un proceso válido en esta cola de prioridad
        while (!q_is_empty(ready_queue[priority])) {
            pid_t next_pid = (pid_t) q_poll(ready_queue[priority]);
            if (!pid_is_valid(next_pid)) {
                continue;
            }
            PCB *candidate = processes[next_pid];
            if (candidate != NULL && candidate->status == PS_READY) {
                return candidate;
            }
            // Si el proceso ya no está listo, seguimos buscando en esta cola
        }
    }

    return NULL;
}


// ============================================
//              AGING SYSTEM
// ============================================

// Aplica aging: promueve procesos que llevan mucho tiempo sin correr
static void apply_aging(void) {
    // Recorrer desde MIN_PRIORITY hasta MAX_PRIORITY+1 (no promovemos desde MAX_PRIORITY)
    for (int i = MIN_PRIORITY; i > MAX_PRIORITY; i--) {
        if (q_is_empty(ready_queue[i])) {
            continue;
        }
        
        q_to_begin(ready_queue[i]);
        while (q_has_next(ready_queue[i])) {
            pid_t pid = q_next(ready_queue[i]);
            PCB *p = processes[pid];
            
            // Si el proceso no existe o no cumple el threshold, continuar
            if (p == NULL) {
                continue;
            }
            
            // Verificar si hace mucho que no corre (comparar con total_cpu_ticks)
            if (total_cpu_ticks - p->last_tick >= AGING_THRESHOLD) {
                // Promover: remover de esta cola y agregar a la de mayor prioridad
                q_remove_current(ready_queue[i]);
                p->effective_priority = i - 1;  // Subir un nivel de prioridad
                p->last_tick = total_cpu_ticks; // Actualizar last_tick para evitar promociones repetidas
                q_add(ready_queue[i - 1], pid);
                
            }
        }
    }
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
    if (!pid_is_valid(pid)) {
        return -1;
    }

    PCB *process = proc_create(pid, entry, argc, argv, name, true, fds);
    if (process == NULL) {
        return -1;
    }

    // Inicializar campos relacionados con scheduling
    process->priority = DEFAULT_PRIORITY; // Asignar prioridad por defecto
    process->effective_priority = DEFAULT_PRIORITY; // Inicialmente igual a priority
    process->status = PS_READY; 
    process->cpu_ticks = 0;
    process->last_tick = total_cpu_ticks;


    processes[pid] = process;
    process_count++;

    if (!q_add(ready_queue[process->effective_priority], pid)) {
        processes[pid] = NULL;
        process_count--;
        free_process_resources(process);
        return -1;
    }

    return pid;
}

int scheduler_remove_process(pid_t pid) {
    if (!scheduler_initialized || !pid_is_valid(pid)) {
        return -1;
    }

    PCB *process = processes[pid];
    if (!process) {
        return -1;
    }

    // Remover de la cola de procesos listos para correr
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        // Buscar en la cola correspondiente a su prioridad efectiva
        q_remove(ready_queue[process->effective_priority], process->pid);
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
   if (!scheduler_initialized || !pid_is_valid(pid) || 
        processes[pid] == NULL || 
        new_priority < MAX_PRIORITY || new_priority > MIN_PRIORITY) { // mas chico el numero, mayor la prioridad
        return -1;
    }

    PCB *process = processes[pid];
    uint8_t old_priority = process->priority;
    uint8_t old_effective_priority = process->effective_priority;
    
    // Si la prioridad no cambia, no hacer nada
    if (old_priority == new_priority) {
        return 0;
    }
    
    // Si el proceso está READY, hay que moverlo de una cola a otra
    if (process->status == PS_READY) { 
        // Remover de la cola actual (usa effective_priority porque ahí está realmente)
        q_remove(ready_queue[old_effective_priority], pid);
        
        // Cambiar ambas prioridades
        process->priority = new_priority;
        process->effective_priority = new_priority;
        
        // Actualizar last_tick: el cambio manual de prioridad cuenta como "atención" del scheduler
        process->last_tick = total_cpu_ticks;
        
        // Agregar a la nueva cola
        if (!q_add(ready_queue[new_priority], pid)) {
            // Si falla, volver a poner en la cola antigua
            process->priority = old_priority;
            process->effective_priority = old_effective_priority;
            q_add(ready_queue[old_effective_priority], pid);
            return -1;
        }
    } else {
        // Si está RUNNING, BLOCKED o TERMINATED, solo cambiar la prioridad base
        // effective_priority se reseteará cuando vuelva a correr
        process->priority = new_priority;
        process->effective_priority = new_priority;
    }
    
    return 0;
}

int scheduler_get_priority(pid_t pid) {
    if (!scheduler_initialized || !pid_is_valid(pid) || processes[pid] == NULL) {
        return -1;
    }
    PCB *process = processes[pid];
    return process->priority;
}

void scheduler_force_reschedule(void) {
    if (scheduler_initialized) {
        force_reschedule = true;
        timer_tick();
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
    
    if (!pid_is_valid(pid)) {
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
    close_open_fds(killed_process);


    if(killed_process->parent_pid == INIT_PID) {
        scheduler_remove_process(killed_process->pid); 
    } else { // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (killed_process->status == PS_READY || killed_process->status == PS_RUNNING) {
            q_remove(ready_queue[killed_process->effective_priority], killed_process->pid);
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
    if (!scheduler_initialized || !pid_is_valid(pid)) {
        return -1;
    }
    
    PCB *process = processes[pid];
    if (process == NULL) {
        return -1;
    }
    
    // Remover de cola READY (si está ahí)
    if (process->status == PS_READY || process->status == PS_RUNNING) {
        q_remove(ready_queue[process->effective_priority], process->pid);
    }
    
    process->status = PS_BLOCKED;
    
    // Si es el proceso actual, forzar reschedule
    if (pid == current_pid) {
        scheduler_force_reschedule();
    }
    
    return 0;
}


int scheduler_unblock_process(pid_t pid) {
    if (!scheduler_initialized || !pid_is_valid(pid)) {
        return -1;
    }
    
    PCB *process = processes[pid];
    if (!process || process->status != PS_BLOCKED) {
        return -1;
    }
    
    process->status = PS_READY;
    
    // Al desbloquearse, el proceso vuelve a su prioridad base
    // (por si fue promovido por aging mientras esperaba ser seleccionado, antes de bloquearse)
    process->effective_priority = process->priority;
    
    // Actualizar last_tick: el tiempo bloqueado no cuenta para aging
    // El proceso debe esperar en READY para acumular tiempo y ser promovido
    process->last_tick = total_cpu_ticks;
    
    // Agregar a la cola correspondiente a su prioridad efectiva
    if (!q_add(ready_queue[process->effective_priority], pid)) {
        process->status = PS_BLOCKED;
        return -1;
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

void scheduler_destroy(void) {
    if (!scheduler_initialized) {
        return;
    }

    // Limpiar todas las colas de prioridades liberando nodos
    for (int i = MAX_PRIORITY; i <= MIN_PRIORITY; i++) {
        q_destroy(ready_queue[i]);
        ready_queue[i] = NULL;
    }

    cleanup_all_processes();

    process_count = 0;
    total_cpu_ticks = 0;
    force_reschedule = false;
    current_pid = NO_PID;
    scheduler_initialized = false;
}

// En scheduler.c
PCB *scheduler_get_process(pid_t pid) {
    if (!scheduler_initialized || !pid_is_valid(pid)) {
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
    close_open_fds(current_process);

    if(current_process->parent_pid == INIT_PID) { // si el padre es init, no hace falta mantener su pcb para guardarnos ret_value pues nadie le va a hacer waitpid
        scheduler_remove_process(current_process->pid); 
    } else{ // si el padre no es init, no vamos a eliminarlo porque su padre podria hacerle un wait
        // Lo sacamos de la cola de procesos ready para que no vuelva a correr PERO NO  del array de procesos (para que el padre pueda acceder a su ret_value)
        if (current_process->status == PS_READY || current_process->status == PS_RUNNING) {
            q_remove(ready_queue[current_process->effective_priority], current_process->pid);
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
    if (!scheduler_initialized || !pid_is_valid(child_pid) || 
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
    if(!scheduler_initialized || !pid_is_valid(pid)) {
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
    if(!scheduler_initialized || !pid_is_valid(pid)) {
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
