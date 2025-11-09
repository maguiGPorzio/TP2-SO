// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "synchro.h"
#include "scheduler.h"
#include "memoryManager.h"
#include "lib.h"
#include "process.h"
#include "videoDriver.h"
#include "ready_queue.h"

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
static int64_t sem_close_by_pid(char *name, uint32_t pid);

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
            return ERROR; // El proceso ya posee este semáforo
        }
        acquire_lock(&sem->lock);
        sem->owner_pids[scheduler_get_current_pid()] = 1;
        sem->ref_count++;
        release_lock(&sem->lock);
        return OK;
    }
    
    // Buscar slot libre
    int64_t id = get_free_id();
    if (id == -1) {
        return ERROR;
    }
    
    // Crear nuevo semáforo
    MemoryManagerADT mm = get_kernel_memory_manager();
    sem = alloc_memory(mm, sizeof(semaphore_t));
    if (sem == NULL) {
        return ERROR;
    }
    
    // Inicializar
    sem->value = initial_value;
    strncpy(sem->name, name, MAX_SEM_NAME_LENGTH - 1);
    sem->name[MAX_SEM_NAME_LENGTH - 1] = '\0';
    sem->queue.read_index = 0;
    sem->queue.write_index = 0;
    sem->queue.size = 0;
    sem->lock = 1;  // Spinlock desbloqueado
    sem->ref_count = 1;
    for (int i=0 ; i<MAX_PROCESSES ; i++) {
        sem->owner_pids[i] = FREE;
    }
    sem->owner_pids[scheduler_get_current_pid()] = OCCUPIED;
    
    sem_manager->semaphores[id] = sem;
    sem_manager->semaphore_count++;
    
    return OK;
}

int64_t sem_close(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return ERROR;
    }
    
    int idx = get_idx_by_name(name);
    if (idx == ERROR) {
        return ERROR;
    }
    
    semaphore_t *sem = sem_manager->semaphores[idx];
    
    acquire_lock(&sem->lock);
    
    if (sem->ref_count > 1) {
        sem->ref_count--;
        sem->owner_pids[scheduler_get_current_pid()] = FREE;
        release_lock(&sem->lock);
        return OK;
    }
    
    release_lock(&sem->lock);
    
    // Último proceso usando el semáforo, destruirlo
    MemoryManagerADT mm = get_kernel_memory_manager();
    free_memory(mm, sem);
    sem_manager->semaphores[idx] = NULL;
    sem_manager->semaphore_count--;
    
    return OK;
}

int64_t sem_wait(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return ERROR;
    }
    
    semaphore_t *sem = get_sem_by_name(name);
    if (sem == NULL) {
        return ERROR;
    }
    
    acquire_lock(&sem->lock);
    
    if (sem->value > 0) {
        sem->value--;
        release_lock(&sem->lock);
        return OK;
    }
    
    // No hay recursos disponibles, bloquear proceso
    int pid = scheduler_get_current_pid();
    
    if (add_to_queue(sem, pid) == ERROR) {
        release_lock(&sem->lock);
        return ERROR;
    }

    _cli(); //deshabilitar interrupciones

    release_lock(&sem->lock);

    scheduler_block_process(pid);
    
    _sti(); //habilitar interrupciones
    
    return 0;
}


int64_t sem_post(char *name) {
    if (sem_manager == NULL || name == NULL) {
        return ERROR;
    }
    
    semaphore_t *sem = get_sem_by_name(name);
    if (sem == NULL) {
        return ERROR;
    }
    
    acquire_lock(&sem->lock);
    
    if (sem->queue.size > 0) {
        // Hay procesos esperando, desbloquear uno
        uint32_t pid = pop_from_queue(sem);
         _cli(); //deshabilitar interrupciones
        release_lock(&sem->lock);
        scheduler_unblock_process(pid);
        _sti(); //habilitar interrupciones
    } else {
        // No hay procesos esperando, incrementar contador
        sem->value++;
        release_lock(&sem->lock);
    }
    
    return OK;
}

int remove_process_from_all_semaphore_queues(uint32_t pid) {
    if (sem_manager == NULL) {
        return ERROR;
    }
    
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        semaphore_t *sem = sem_manager->semaphores[i];
        if (sem == NULL) {
            continue;
        }

        if (sem->owner_pids[pid]==OCCUPIED){
            if (scheduler_get_process(pid)->status == PS_BLOCKED) {
                acquire_lock(&sem->lock);
                remove_process_from_queue(sem, pid);
                release_lock(&sem->lock);
            }
            sem_close_by_pid(sem->name, pid);
        }
    }
    
    return OK;
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
    return ERROR;
}

static uint64_t pop_from_queue(semaphore_t *sem) {
    if (sem->queue.size == 0) {
        return ERROR;
    }
    
    uint32_t pid = sem->queue.pids[sem->queue.read_index];
    sem->queue.read_index = (sem->queue.read_index + 1) % MAX_PROCESSES;
    sem->queue.size--;
    
    return pid;
}

static int add_to_queue(semaphore_t *sem, uint32_t pid) {
    if (sem->queue.size >= MAX_PROCESSES) {
        return ERROR;
    }
    
    sem->queue.pids[sem->queue.write_index] = pid;
    sem->queue.write_index = (sem->queue.write_index + 1) % MAX_PROCESSES;
    sem->queue.size++;
    
    return OK;
}

static int get_idx_by_name(const char *name) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_manager->semaphores[i] != NULL &&
            strcmp(sem_manager->semaphores[i]->name, name) == 0) {
            return i;
        }
    }
    return ERROR;
}

static semaphore_t *get_sem_by_name(const char *name) {
    int idx = get_idx_by_name(name);
    if (idx == ERROR) {
        return NULL;
    }
    return sem_manager->semaphores[idx];
}

static int remove_process_from_queue(semaphore_t *sem, uint32_t pid) {
    if (sem->queue.size == 0) {
        return ERROR;
    }
    
    uint32_t i = sem->queue.read_index;
    uint32_t j = 0;
    int found = ERROR;
    
    // Buscar el proceso en la cola
    while (j < sem->queue.size) {
        if (sem->queue.pids[i] == pid) {
            found = i;
            break;
        }
        i = (i + 1) % MAX_PROCESSES;
        j++;
    }
    
    if (found == ERROR) {
        return ERROR;  // No encontrado
    }
    
    // Desplazar elementos para llenar el hueco
    for (uint32_t k = found; k != sem->queue.write_index; k = (k + 1) % MAX_PROCESSES) {
        uint32_t next = (k + 1) % MAX_PROCESSES;
        sem->queue.pids[k] = sem->queue.pids[next];
    }

    //Es un truco matemático para evitar números negativos al retroceder en aritmética circular.
    sem->queue.write_index = (sem->queue.write_index + MAX_PROCESSES - 1) % MAX_PROCESSES;
    sem->queue.size--;
    
    return OK;
}

static int64_t sem_close_by_pid(char *name, uint32_t pid) {
    if (sem_manager == NULL || name == NULL) {
        return ERROR;
    }
    
    int idx = get_idx_by_name(name);
    if (idx == ERROR) {
        return ERROR;
    }
    
    semaphore_t *sem = sem_manager->semaphores[idx];
    
    acquire_lock(&sem->lock);
    
    if (sem->ref_count > 1) {
        sem->ref_count--;
        sem->owner_pids[pid] = FREE;
        release_lock(&sem->lock);
        return OK;
    }
    
    release_lock(&sem->lock);
    
    // Ultimo proceso usando el semaforo, destruirlo
    MemoryManagerADT mm = get_kernel_memory_manager();
    free_memory(mm, sem);
    sem_manager->semaphores[idx] = NULL;
    sem_manager->semaphore_count--;
    
    return OK;
}