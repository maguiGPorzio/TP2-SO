// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "memory_manager.h"
#include <stdbool.h>
#include <stdint.h>
#include "naiveConsole.h"

#define MIN_ORDER 5        // 2^5 = 32 bytes (tamaño mínimo)
#define MAX_ORDER 25       // 2^25 = 32 MB (tamaño máximo de bloque)
#define NUM_ORDERS (MAX_ORDER - MIN_ORDER + 1)  // 21 niveles

// Nodo de la lista libre para cada orden
typedef struct buddy_node_t {
    struct buddy_node_t* next;  // Siguiente en la lista libre
    struct buddy_node_t* prev;  // Anterior en la lista libre
    bool free;               // ¿Está libre?
    uint8_t order;          // Orden del bloque (2^order bytes)
} buddy_node_t;

// Estructura del Buddy Memory Manager
struct memory_manager_CDT {
    void* base_address;                    // Dirección base de la memoria
    size_t total_size;                     // Tamaño total
    buddy_node_t* free_lists[NUM_ORDERS];    // Array de listas libres por orden
    size_t allocated_blocks;               // Bloques allocados
    size_t total_allocated;                // Bytes totales allocados
};

static memory_manager_ADT kernel_mm = NULL;

extern void _hlt(void);

// Calcula el tamaño de un bloque dado su orden
static size_t order_to_size(uint8_t order) {
    return (1ULL << order); // 2^order
}

// Calcula el orden necesario para un tamaño dado
static uint8_t size_to_order(size_t size) {
    // Incluir espacio para el header
    size += sizeof(buddy_node_t); 
    
    uint8_t order = MIN_ORDER; 
    size_t block_size = order_to_size(order);
    
    // Buscar el orden más pequeño que entre
    while (block_size < size && order < MAX_ORDER) {
        order++;
        block_size = order_to_size(order);
    }

    if (order > MAX_ORDER) {
        return 0xFF; // Indicar que no hay suficiente espacio
    }
    
    return order;
}

// Calcula la dirección del buddy de un bloque
static void* get_buddy_address(memory_manager_ADT memory_manager, void* block_addr, uint8_t order) {
    // XOR con el tamaño del bloque para flip el bit correspondiente
    size_t offset = (char*)block_addr - (char*)memory_manager->base_address;
    size_t buddy_offset = offset ^ order_to_size(order);
    return (char*)memory_manager->base_address + buddy_offset;
}

// Elimina un nodo de su lista libre
static void remove_from_free_list(memory_manager_ADT memory_manager, buddy_node_t* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        // Es el primer nodo de la lista
        memory_manager->free_lists[index] = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
    
    node->next = NULL;
    node->prev = NULL;
}

// Agrega un nodo al inicio de su lista libre
static void add_from_free_list(memory_manager_ADT memory_manager, buddy_node_t* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    node->next = memory_manager->free_lists[index];
    node->prev = NULL;
    node->order = order;
    node->free = true;
    
    if (memory_manager->free_lists[index]) {
        memory_manager->free_lists[index]->prev = node;
    }
    
    memory_manager->free_lists[index] = node;
}

// Divide un bloque en dos buddies
static buddy_node_t* split_block(memory_manager_ADT memory_manager, uint8_t order) {
    if (order == MIN_ORDER) {
        return NULL;  // No se puede dividir más
    }
    
    if (order > MAX_ORDER || order < MIN_ORDER) {
        return NULL;  // Orden inválido
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques de este orden, dividir uno más grande
    if (memory_manager->free_lists[index] == NULL && order < MAX_ORDER) {
        if (split_block(memory_manager, order + 1) == NULL) {
            return NULL;  // No hay bloques más grandes
        }
    }

    if (memory_manager->free_lists[index] == NULL) {
        return NULL;  // No hay bloques de este orden
    }
    
    // Tomar el primer bloque de este orden
    buddy_node_t* block = memory_manager->free_lists[index];
    remove_from_free_list(memory_manager, block, order);
    
    // Dividir en dos buddies de orden-1
    uint8_t new_order = order - 1;
    size_t half_size = order_to_size(new_order);
    
    // Primer buddy (el bloque original)
    buddy_node_t* buddy1 = block;
    
    // Segundo buddy (mitad superior)
    buddy_node_t* buddy2 = (buddy_node_t*)((char*)block + half_size);
    
    // Agregar ambos a la lista del orden inferior
    add_from_free_list(memory_manager, buddy1, new_order);
    add_from_free_list(memory_manager, buddy2, new_order);
    
    return buddy1;
}

// Fusiona un bloque con su buddy si es posible
static void coalesce(memory_manager_ADT memory_manager, buddy_node_t* block) {
    uint8_t order = block->order;
    
    if (order >= MAX_ORDER) {
        return;  // Ya es el bloque más grande
    }
    
    // Buscar el buddy
    void* buddyAddr = get_buddy_address(memory_manager, block, order);
    buddy_node_t* buddy = (buddy_node_t*)buddyAddr;
    
    // Verificar si el buddy está libre y tiene el mismo orden
    if (!buddy->free || buddy->order != order) {
        return;  // No se puede fusionar
    }
    
    // Eliminar ambos de sus listas
    remove_from_free_list(memory_manager, block, order);
    remove_from_free_list(memory_manager, buddy, order);
    
    // El bloque con dirección menor se convierte en el bloque fusionado
    buddy_node_t* merged = (block < buddy) ? block : buddy;
    
    // Agregar a la lista del siguiente orden
    add_from_free_list(memory_manager, merged, order + 1);
    
    // Intentar fusionar recursivamente
    coalesce(memory_manager, merged);
}

memory_manager_ADT create_memory_manager(void* start_address, size_t size) {
    if (start_address == NULL || size < sizeof(struct memory_manager_CDT) + (1ULL << MIN_ORDER)) {
        return NULL;
    }
    
    // Colocar el CDT al inicio
    memory_manager_ADT memory_manager = (memory_manager_ADT)start_address;
    memory_manager->base_address = (char*)start_address + sizeof(struct memory_manager_CDT);
    memory_manager->total_size = size - sizeof(struct memory_manager_CDT);
    memory_manager->allocated_blocks = 0;
    memory_manager->total_allocated = 0;
    
    // Inicializar listas libres
    for (int i = 0; i < NUM_ORDERS; i++) {
        memory_manager->free_lists[i] = NULL;
    }
    
    // Crear bloques iniciales con la memoria disponible
    size_t remaining_size = memory_manager->total_size;
    void* current_address = memory_manager->base_address;
    uint8_t order = MAX_ORDER;
    size_t block_size = order_to_size(order);
    
    // Dividir la memoria en bloques del máximo orden posible
    while (remaining_size >= (1ULL << MIN_ORDER)) {
        
        // Encontrar el bloque más grande que cabe
        while (block_size > remaining_size && order > MIN_ORDER) {
            order--;
            block_size = order_to_size(order);
        }
        
        // Crear el bloque
        buddy_node_t* node = (buddy_node_t*)current_address;
        add_from_free_list(memory_manager, node, order);  // ← Ahora pasa memory_manager
        
        current_address = (char*)current_address + block_size;
        remaining_size -= block_size;
    }
    
    return memory_manager;
}

void* alloc_memory(memory_manager_ADT memory_manager, size_t size) {
    if (memory_manager == NULL || size == 0) {
        return NULL;
    }
    
    // Calcular el orden necesario
    uint8_t order = size_to_order(size);
    
    if (order > MAX_ORDER) {
        return NULL;  // Demasiado grande
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques del orden exacto, dividir uno más grande
    if (memory_manager->free_lists[index] == NULL && order < MAX_ORDER) {
        if (split_block(memory_manager, order + 1) == NULL) {
            return NULL;  // Sin memoria
        }
    }

    if (memory_manager->free_lists[index] == NULL) {
        return NULL;  // Sin memoria
    }
    
    // Tomar el primer bloque del orden
    buddy_node_t* block = memory_manager->free_lists[index];
    remove_from_free_list(memory_manager, block, order);
    
    // Marcar como ocupado
    block->free = false;
    block->order = order;
    
    memory_manager->allocated_blocks++;
    memory_manager->total_allocated += order_to_size(order) - sizeof(buddy_node_t);
    
    // Retornar puntero después del header
    return (char*)block + sizeof(buddy_node_t);
}

void free_memory(memory_manager_ADT memory_manager, void* ptr) {
    if (memory_manager == NULL || ptr == NULL) {
        return;
    }
    
    // Obtener el bloque
    buddy_node_t* block = (buddy_node_t*)((char*)ptr - sizeof(buddy_node_t));
    
    if (block->free) {
        return;  // Double free
    }
    
    // Marcar como libre
    block->free = true;
    memory_manager->allocated_blocks--;
    memory_manager->total_allocated -= order_to_size(block->order) - sizeof(buddy_node_t);
    
    // Agregar a la lista libre
    add_from_free_list(memory_manager, block, block->order);
    
    // Intentar fusionar con el buddy
    coalesce(memory_manager, block);
}

mem_info_t get_mem_status(memory_manager_ADT memory_manager) {
    mem_info_t status = {0};
    
    if (memory_manager != NULL) {
        status.total_memory = memory_manager->total_size;
        status.used_memory = memory_manager->total_allocated;
        status.free_memory = status.total_memory - status.used_memory;
        status.allocated_blocks = memory_manager->allocated_blocks;
    }
    
    return status;
}

// TODO: esto no lo usamos
void print_mem_state(memory_manager_ADT memory_manager) {
    if (memory_manager == NULL) {
        return;
    }
    
    for (int i = 0; i < NUM_ORDERS; i++) {
        uint8_t order = MIN_ORDER + i;
        int count = 0;
        buddy_node_t* node = memory_manager->free_lists[i];
        
        while (node != NULL) {
            count++;
            node = node->next;
        }
    }
}

void init_kernel_memory_manager(void) {
    kernel_mm = create_memory_manager((void*)HEAP_START_ADDRESS, HEAP_SIZE);
    
    if (kernel_mm == NULL) {
        while(1) {
            _hlt();
        }
    }
}

memory_manager_ADT get_kernel_memory_manager(void) {
    return kernel_mm;
}

void* sys_malloc(size_t size) {
    return alloc_memory(kernel_mm, size);
}

void sys_free(void* ptr) {
    free_memory(kernel_mm, ptr);
}

mem_info_t sys_mem_info(void) {
    return get_mem_status(kernel_mm);
}