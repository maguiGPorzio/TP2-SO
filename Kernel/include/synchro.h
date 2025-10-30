#ifndef SYNCHRO_H
#define SYNCHRO_H

#include <stdint.h>

#define MAX_SEMAPHORES 256
#define MAX_SEM_NAME_LENGTH 64

typedef int lock_t;

extern void acquire_lock(lock_t *lock);
extern void release_lock(lock_t *lock);

//Inicializa el sistema de semáforos al arrancar el kernel.
void init_semaphore_manager(void);
// Crea o abre un semáforo con un nombre dado.
// Aloca memoria para el manager de semáforos
// Inicializa el array de semáforos en NULL
// Se llama UNA vez al inicio del kernel
int64_t sem_open(char *name, int initial_value);
//Cierra un semáforo con un nombre dado.
// Si hay más procesos usándolo → solo decrementa contador
// Si es el último proceso → destruye el semáforo y libera memoria
// Retorna: 0 si éxito, -1 si error
int64_t sem_close(char *name);
// Intentar adquirir recurso.
// Si value > 0 → decrementa valor y continúa
// Si value == 0 → BLOQUEA el proceso y lo pone en cola de espera
// Retorna: 0 si éxito, -1 si error
int64_t sem_wait(char *name);
// Liberar recurso.
// Si hay procesos esperando → DESBLOQUEA uno de la cola
// Si NO hay procesos esperando → incrementa el valor
// Retorna: 0 si éxito, -1 si error
// Ejemplo: Salir de sección crítica
int64_t sem_post(char *name);
// Limpia un proceso de TODAS las colas de semáforos.
// Se llama cuando un proceso termina o es matado
// Evita que queden PIDs zombies bloqueados en semáforos
// Retorna: 0 si éxito, -1 si error
int remove_process_from_all_semaphore_queues(uint32_t pid);

#endif