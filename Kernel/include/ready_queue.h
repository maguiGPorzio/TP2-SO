#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "process.h"
#include <memoryManager.h>

// Forward declaration de nodo interno
typedef struct RQNode RQNode;

// Cola circular de procesos READY (una por prioridad)
typedef struct ReadyQueue {
    RQNode *head;    // Primer nodo en la cola
    RQNode *current; // Nodo actual en rotación Round Robin
    int count;       // Cantidad de procesos en esta cola
} ReadyQueue;

// Inicializa una ReadyQueue (poner a cero sus campos)
void ready_queue_init(ReadyQueue *queue);

// Operaciones de la cola
void ready_queue_enqueue(ReadyQueue *queue, PCB *process);
void ready_queue_dequeue(ReadyQueue *queue, PCB *process);
PCB *ready_queue_next(ReadyQueue *queue);

// Liberar todos los nodos de la cola (usa el memory manager interno)
void ready_queue_destroy(ReadyQueue *queue);

#endif // READY_QUEUE_H
