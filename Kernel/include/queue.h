#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct queue_cdt * queue_t;

// constructor
queue_t q_init();

// devuelve 1 si lo agrego, 0 sino (si se puede cambiar a bool)
int q_add(queue_t q, int value);

// son pids (>= 0) devuelve -1 si la lista esta vacia
int q_poll(queue_t q);

// elemina la primer aparicion de value
int q_remove(queue_t q, int value);

// devuelve 1 si el value esta en la queue, 0 sino
int q_contains(queue_t q, int value);

// devuelve 1 si esta vacia, 0 sino
int q_is_empty(queue_t q);

// libera los recursos de la queue
void q_destroy(queue_t q); 

// ============================================
//             ITERADOR
// ============================================

// Inicializa el iterador al comienzo de la queue
void q_to_begin(queue_t q);

// Devuelve 1 si hay un siguiente elemento, 0 sino
int q_has_next(queue_t q);

// Devuelve el elemento actual y avanza al siguiente
// Si no hay siguiente, retorna -1
int q_next(queue_t q);

// Remueve el elemento actual del iterador (el último retornado por q_next)
// Devuelve 1 si lo removió, 0 si no había elemento actual
int q_remove_current(queue_t q);

#endif 