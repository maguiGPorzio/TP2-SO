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

// devuelve 1 si esta vacia, 0 sino
int q_is_empty(queue_t q);

// libera los recursos de la queue
void q_destroy(queue_t q); 

#endif 