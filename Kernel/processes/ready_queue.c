// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "ready_queue.h"
#include "lib.h"
#include <memoryManager.h>

// Nodo interno para la lista circular 
typedef struct RQNode {
    PCB *proc;
    struct RQNode *next;
    struct RQNode *prev;
} RQNode;

void ready_queue_init(ReadyQueue *queue) {
    if (queue == NULL) return;
    queue->head = NULL;
    queue->current = NULL;
    queue->count = 0;
}

// Te pasan la cola de la prioridad
void ready_queue_enqueue(ReadyQueue *queue, PCB *process) {
    if (queue == NULL || process == NULL) return;

    MemoryManagerADT mm = get_kernel_memory_manager();
    RQNode *node = alloc_memory(mm, sizeof(RQNode));
    if (node == NULL) return; // sin memoria

    node->proc = process;
    if (queue->head == NULL) { // Primera inserción: lista circular de 1 elemento
        node->next = node;
        node->prev = node;
        queue->head = node;
        queue->current = node;
    } else {
        // TODO: leer bien esto
        // Insertar al final de la lista circular (antes del head)
        RQNode *tail = queue->head->prev;
        tail->next = node;
        node->prev = tail;
        node->next = queue->head;
        queue->head->prev = node;
    }

    queue->count++;
}

void ready_queue_dequeue(ReadyQueue *queue, PCB *process) {
    if (queue == NULL || process == NULL || queue->count == 0) return;

    // Buscar el nodo correspondiente
    RQNode *n = queue->head;
    RQNode *found = NULL;
    for (int i = 0; i < queue->count; i++) {
        if (n->proc == process) { found = n; break; }
        n = n->next;
    }
    if (found == NULL) return; // no estaba en la cola

    if (queue->count == 1) { // Último elemento
        queue->head = NULL;
        queue->current = NULL;
    } else {
        // Desenlazar el nodo de la lista
        found->prev->next = found->next;
        found->next->prev = found->prev;
        // Actualizar head y current si es necesario
        if (queue->head == found) {
            queue->head = found->next;
        }
        if (queue->current == found) {
            queue->current = found->next;
        }
    }

    // Liberar el nodo
    MemoryManagerADT mm = get_kernel_memory_manager();
    free_memory(mm, found);
    queue->count--;
}

// Obtener el siguiente proceso en la cola (round robin)
PCB *ready_queue_next(ReadyQueue *queue) {
    if (queue == NULL || queue->current == NULL || queue->count == 0) return NULL;
    RQNode *result = queue->current;
    queue->current = queue->current->next;
    return result->proc;
}

void ready_queue_destroy(ReadyQueue *queue) {
    if (queue == NULL) return;
    MemoryManagerADT mm = get_kernel_memory_manager();

    int remaining = queue->count;
    RQNode *start = queue->head;
    RQNode *n = start;
    while (remaining-- > 0 && n != NULL) {
        RQNode *next = n->next;
        free_memory(mm, n);
        if (next == start) break;
        n = next;
    }

    queue->head = NULL;
    queue->current = NULL;
    queue->count = 0;
}
