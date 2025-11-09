// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "queue.h"
#include "memoryManager.h"

typedef struct node {
    int value;
    struct node * next;
} node_t;

typedef struct queue_cdt {
    node_t * first;
    node_t * last;
    node_t * current;      // Para iterador: nodo actual
    node_t * prev_current; // Para iterador: nodo previo al actual (para poder remover)
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
    q->current = NULL;
    q->prev_current = NULL;
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

    if (q->first == NULL) { 
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
    if (q->first == NULL) {
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

// devuelve 1 si el value esta en la queue, 0 sino
int q_contains(queue_t q, int value) {
    if (q_is_empty(q)) {
        return 0;
    }
    
    node_t *current = q->first;
    while (current != NULL) {
        if (current->value == value) {
            return 1;
        }
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

// ============================================
//             ITERADOR
// ============================================

// Inicializa el iterador al comienzo de la queue
void q_to_begin(queue_t q) {
    if (q == NULL) {
        return;
    }
    q->current = q->first;
    q->prev_current = NULL;
}

// Devuelve 1 si hay un siguiente elemento, 0 sino
int q_has_next(queue_t q) {
    if (q == NULL) {
        return 0;
    }
    return q->current != NULL;
}

// Devuelve el elemento actual y avanza al siguiente
// Si no hay siguiente, retorna -1
int q_next(queue_t q) {
    if (!q_has_next(q)) {
        return -1;
    }
    
    int value = q->current->value;
    q->prev_current = q->current;
    q->current = q->current->next;
    
    return value;
}

// Remueve el elemento actual del iterador (el último retornado por q_next)
// Devuelve 1 si lo removió, 0 si no había elemento actual
int q_remove_current(queue_t q) {
    if (q == NULL || q->prev_current == NULL) {
        return 0;
    }
    
    MemoryManagerADT mm = get_kernel_memory_manager();
    node_t * to_remove = q->prev_current;
    
    // Caso 1: el nodo a remover es el primero
    if (to_remove == q->first) {
        q->first = to_remove->next;
        if (q->first == NULL) {
            q->last = NULL;
        }
        free_memory(mm, to_remove);
        q->prev_current = NULL;
        return 1;
    }
    
    // Caso 2: el nodo a remover no es el primero
    // Necesitamos encontrar el nodo anterior a to_remove
    node_t * prev = q->first;
    while (prev != NULL && prev->next != to_remove) {
        prev = prev->next;
    }
    
    if (prev == NULL) {
        // No debería pasar si el iterador está bien usado
        return 0;
    }
    
    prev->next = to_remove->next;
    
    // Si removimos el último, actualizar last
    if (to_remove == q->last) {
        q->last = prev;
    }
    
    free_memory(mm, to_remove);
    q->prev_current = NULL;
    
    return 1;
}
