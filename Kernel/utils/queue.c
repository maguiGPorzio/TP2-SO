#include "queue.h"
#include "memoryManager.h"

typedef struct node {
    int value;
    struct node * next;
} node_t;

typedef struct queue_cdt {
    node_t * first;
    node_t * last;
} queue_cdt;



// constructor
queue_t q_init() {
    MemoryManagerADT mm = get_kernel_memory_manager();
    queue_t q = alloc_memory(mm, sizeof(queue_cdt));
    if (q == NULL) {
        return NULL;
    }
    q->first = NULL;
    q->last = NULL;
    return q;
}

// devuelve 1 si lo agrego, 0 sino (si se puede cambiar a bool)
int q_add(queue_t q, int value) {
    MemoryManagerADT mm = get_kernel_memory_manager();
    node_t * new_node = alloc_memory(mm, sizeof(node_t));
    if (new_node == NULL) {
        return 0;
    }
    new_node->value = value;
    new_node->next = NULL;

    if (q_is_empty(q)) { 
        q->first = new_node;
        q->last = new_node;
        return 1;
    }

    q->last->next = new_node;
    q->last = new_node;
    return 1;
}

// son pids (>= 0) devuelve -1 si la lista esta vacia
int q_poll(queue_t q) {
    if (q_is_empty(q)) {
        return -1;
    }
    MemoryManagerADT mm = get_kernel_memory_manager();
    int res = q->first->value;
    node_t * to_free = q->first;
    q->first = to_free->next;
    free_memory(mm, to_free);
    if (q_is_empty(q)) {
        q->last = NULL;
    }

    return res;
}

// elemina la primer aparicion de value, devuelve 1 si lo removio, 0 sino
int q_remove(queue_t q, int value) {
    if (q_is_empty(q)) {
        return 0;
    }
    MemoryManagerADT mm = get_kernel_memory_manager();
    if (q->first->value == value) {
        node_t * to_free = q->first;
        q->first = q->first->next;
        free_memory(mm, to_free);
        if (q_is_empty(q)) {
            q->last = NULL;
        }
        return 1;        
    }
    node_t * prev = q->first;
    node_t * current = q->first->next;
    while (current != NULL) {
        if (current->value == value) {
            prev->next = current->next;
            free_memory(mm, current);
            if (prev->next == NULL) {
                q->last = prev;
            }
            return 1;
        }
        prev = current;
        current = current->next;
    }

    return 0;

}

// devuelve 1 si esta vacia, 0 sino
int q_is_empty(queue_t q) {
    return q->first == NULL;
}

// libera los recursos de la queue
void q_destroy(queue_t q) {
    if (q == NULL) {
        return;
    }

    MemoryManagerADT mm = get_kernel_memory_manager();
    node_t * current = q->first;
    while (current != NULL) {
        node_t * next = current->next;
        free_memory(mm, current);
        current = next;
    }
    free_memory(mm, q);
}
