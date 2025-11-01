#include "synchro.h"
#include "scheduler.h"
#include "memoryManager.h"
#include "lib.h"
#include "process.h"
#include "videoDriver.h"

// ============================================
//           ESTRUCTURAS INTERNAS
// ============================================

typedef struct {
    uint32_t pids[MAX_PROCESSES]; // Array de PIDs circular
    uint32_t read_index; // Índice de lectura (frente de la cola) (donde sacas el próximo elemento)
    uint32_t write_index; // Índice de escritura (final de la cola) (donde pones el próximo elemento)
    uint32_t size;  // Cantidad de elementos en la cola
} circular_buffer_t;

typedef struct {
    int total_value;  // Valor del semáforo
    int value; // Contador del semáforo
    //1 libre
    //0 ocupado
    uint32_t owner_pids[MAX_PROCESSES]; // PIDs de procesos que poseen el semáforo
    int ref_count;  // Cantidad de procesos usando este semáforo
    char name[MAX_SEM_NAME_LENGTH]; // Nombre del semáforo
    circular_buffer_t queue;
    //1 libre
    //0 ocupado
    int lock;  // Spinlock simple para proteger acceso concurrente
} semaphore_t;

typedef struct {
    semaphore_t *semaphores[MAX_SEMAPHORES]; // Array de punteros a semáforos
    int semaphore_count; // Cantidad de semáforos activos
} semaphore_manager_t;

// ============================================
//         VARIABLES GLOBALES
// ============================================

static semaphore_manager_t *sem_manager = NULL;

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static int64_t get_free_id(void);
static uint64_t pop_from_queue(semaphore_t *sem);
static int add_to_queue(semaphore_t *sem, uint32_t pid);
static semaphore_t *get_sem_by_name(const char *name);
static int get_idx_by_name(const char *name);
static int remove_process_from_queue(semaphore_t *sem, uint32_t pid);

static int pid_present_in_semaphore(semaphore_t *sem, uint32_t pid) {
    return sem->owner_pids[pid];
}

// ============================================
//           INICIALIZACIÓN
// ============================================

void init_semaphore_manager(void) {
    if (sem_manager != NULL) {
        return;  // Ya inicializado
    }
    
    MemoryManagerADT mm = get_kernel_memory_manager();
    sem_manager = alloc_memory(mm, sizeof(semaphore_manager_t));
    
    if (sem_manager == NULL) {
        return;  // Error crítico
    }
    
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        sem_manager->semaphores[i] = NULL;
    }
    
    sem_manager->semaphore_count = 0;
}

// ============================================
//        FUNCIONES PÚBLICAS
// ============================================

int64_t sem_open(char *name, int initial_value) {
    if (sem_manager == NULL || name == NULL) {
        return -1;
    }
    
     // 1. Primero verificar si ya existe
    semaphore_t *sem = get_sem_by_name(name);
    if (sem != NULL) {
        // Ya existe, solo incrementar contador si no es un proceso que ya lo creo
        if (pid_present_in_semaphore(sem, scheduler_get_current_pid())) {
            return -1; // El proceso ya posee este semáforo
        }
        acquire_lock(&sem->lock);
        sem->owner_pids[scheduler_get_current_pid()] = 1;
        sem->ref_count++;
        release_lock(&sem->lock);
        return 0;
    }
    
    // Buscar slot libre
    int64_t id = get_free_id();
    if (id == -1) {
        return -1;
    }
    
    // Crear nuevo semáforo
    MemoryManagerADT mm = get_kernel_memory_manager();
    sem = alloc_memory(mm, sizeof(semaphore_t));
    if (sem == NULL) {
        return -1;
    }
    
    // Inicializar
    sem->value = initial_value;
    sem->total_value = initial_value;
    strncpy(sem->name, name, MAX_SEM_NAME_LENGTH - 1);
    sem->name[MAX_SEM_NAME_LENGTH - 1] = '\0';
    sem->queue.read_index = 0;
    sem->queue.write_index = 0;
    sem->queue.size = 0;
    sem->lock = 1;  // Spinlock desbloqueado
    sem->ref_count = 1;
    for (int i=0 ; i<MAX_PROCESSES ; i++) {
        sem->owner_pids[i] = 0;
    }
    sem->owner_pids[scheduler_get_current_pid()] = 1;
    
    sem_manager->semaphores[id] = sem;
    sem_manager->semaphore_count++;
    
    return 0;
}

int64_t sem_close(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return -1;
    }
    
    int idx = get_idx_by_name(name);
    if (idx == -1) {
        return -1;
    }
    
    semaphore_t *sem = sem_manager->semaphores[idx];
    
    acquire_lock(&sem->lock);
    
    if (sem->ref_count > 1) {
        sem->ref_count--;
        sem->owner_pids[scheduler_get_current_pid()] = 0;
        release_lock(&sem->lock);
        return 0;
    }
    
    release_lock(&sem->lock);
    
    // Último proceso usando el semáforo, destruirlo
    MemoryManagerADT mm = get_kernel_memory_manager();
    free_memory(mm, sem);
    sem_manager->semaphores[idx] = NULL;
    sem_manager->semaphore_count--;
    
    return 0;
}

int64_t sem_wait(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return -1;
    }
    
    semaphore_t *sem = get_sem_by_name(name);
    if (sem == NULL) {
        return -1;
    }
    
    acquire_lock(&sem->lock);

    // Caso rápido: hay recurso disponible
    if (sem->value > 0) {
        sem->value--;
        release_lock(&sem->lock);
        return 0;
    }

    // No hay recursos disponibles, encolarse para esperar
    int pid = scheduler_get_current_pid();
    if (add_to_queue(sem, pid) == -1) {
        release_lock(&sem->lock);
        return -1;
    }

    // Re-chequeo anti-race: si en el medio un post incrementó el valor
    // (porque todavía no estábamos bloqueados), consumir el token y evitar bloquear.
    if (sem->value > 0) {
        sem->value--;                       // consumimos el recurso disponible
        remove_process_from_queue(sem, pid);
        release_lock(&sem->lock);
        return 0;
    }

    release_lock(&sem->lock);

    // Bloquear el proceso; será despertado por un post que haga unblock
    scheduler_block_process(pid);
    return 0;
}

int64_t sem_post(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return -1;
    }
    
    semaphore_t *sem = get_sem_by_name(name);
    if (sem == NULL) {
        return -1;
    }
    
    acquire_lock(&sem->lock);

    if (sem->queue.size > 0) {
        // Hay procesos esperando: preferimos despertar a uno.
        // Para evitar la carrera con sem_wait (proceso aún no bloqueado),
        // miramos el estado antes de decidir.
        uint32_t pid_peek = sem->queue.pids[sem->queue.read_index];
        PCB *p = scheduler_get_process(pid_peek);
        if (p != NULL && p->status == PS_BLOCKED) {
            uint32_t pid = pop_from_queue(sem);
            release_lock(&sem->lock);
            scheduler_unblock_process(pid);
        } else {
            // Aún no bloqueado: no sacamos de la cola; dejamos un "token"
            // para que el waiter evite bloquear (re-chequeo en sem_wait).
            if (sem->total_value == 0 || sem->value < sem->total_value) {
                sem->value++;
            }
            release_lock(&sem->lock);
        }
    } else {
        // No hay procesos esperando, incrementar contador
        if (sem->total_value == 0 || sem->value < sem->total_value) {
            sem->value++;
        }
        release_lock(&sem->lock);
    }

    return 0;
}

int remove_process_from_all_semaphore_queues(uint32_t pid) {
    if (sem_manager == NULL) {
        return -1;
    }
    
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        semaphore_t *sem = sem_manager->semaphores[i];
        if (sem == NULL) {
            continue;
        }
        
        acquire_lock(&sem->lock);
        remove_process_from_queue(sem, pid);
        release_lock(&sem->lock);
    }
    
    return 0;
}

// ============================================
//        FUNCIONES AUXILIARES PRIVADAS
// ============================================

static int64_t get_free_id(void) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_manager->semaphores[i] == NULL) {
            return i;
        }
    }
    return -1;
}

static uint64_t pop_from_queue(semaphore_t *sem) {
    if (sem->queue.size == 0) {
        return -1;
    }
    
    uint32_t pid = sem->queue.pids[sem->queue.read_index];
    sem->queue.read_index = (sem->queue.read_index + 1) % MAX_PROCESSES;
    sem->queue.size--;
    
    return pid;
}

static int add_to_queue(semaphore_t *sem, uint32_t pid) {
    if (sem->queue.size >= MAX_PROCESSES) {
        return -1;
    }
    
    sem->queue.pids[sem->queue.write_index] = pid;
    sem->queue.write_index = (sem->queue.write_index + 1) % MAX_PROCESSES;
    sem->queue.size++;
    
    return 0;
}

static int get_idx_by_name(const char *name) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_manager->semaphores[i] != NULL &&
            strcmp(sem_manager->semaphores[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static semaphore_t *get_sem_by_name(const char *name) {
    int idx = get_idx_by_name(name);
    if (idx == -1) {
        return NULL;
    }
    return sem_manager->semaphores[idx];
}

static int remove_process_from_queue(semaphore_t *sem, uint32_t pid) {
    if (sem->queue.size == 0) {
        return -1;
    }
    
    uint32_t i = sem->queue.read_index;
    uint32_t j = 0;
    int found = -1;
    
    // Buscar el proceso en la cola
    while (j < sem->queue.size) {
        if (sem->queue.pids[i] == pid) {
            found = i;
            break;
        }
        i = (i + 1) % MAX_PROCESSES;
        j++;
    }
    
    if (found == -1) {
        return -1;  // No encontrado
    }
    
    // Desplazar elementos para llenar el hueco
    for (uint32_t k = found; k != sem->queue.write_index; k = (k + 1) % MAX_PROCESSES) {
        uint32_t next = (k + 1) % MAX_PROCESSES;
        sem->queue.pids[k] = sem->queue.pids[next];
    }

    //Es un truco matemático para evitar números negativos al retroceder en aritmética circular.
    sem->queue.write_index = (sem->queue.write_index + MAX_PROCESSES - 1) % MAX_PROCESSES;
    sem->queue.size--;
    
    return 0;
}