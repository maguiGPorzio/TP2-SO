// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "memoryManager.h"
#include <stdbool.h>
#include <stdint.h>
#include "naiveConsole.h"

#define MIN_ORDER 5        // 2^5 = 32 bytes (tamaño mínimo)
#define MAX_ORDER 20       // 2^20 = 1 MB (tamaño máximo de bloque)
#define NUM_ORDERS (MAX_ORDER - MIN_ORDER + 1)  // 16 niveles

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

// ============================================
//         VARIABLES GLOBALES
// ============================================

static memory_manager_ADT kernel_mm = NULL;

// ============================================
//          FUNCIONES AUXILIARES
// ============================================

// Calcula el tamaño de un bloque dado su orden
static size_t order_to_size(uint8_t order) {
    return (1ULL << order);
}

// Calcula el orden necesario para un tamaño dado
static uint8_t size_to_order(size_t size) {
    // Incluir espacio para el header
    size += sizeof(buddy_node_t);
    
    uint8_t order = MIN_ORDER;
    size_t block_size = order_to_size(order);
    
    while (block_size < size && order < MAX_ORDER) {
        order++;
        block_size = order_to_size(order);
    }
    
    return order;
}

// Calcula la dirección del buddy de un bloque
static void* getBuddyAddress(memory_manager_ADT memManager, void* blockAddr, uint8_t order) {
    // XOR con el tamaño del bloque para flip el bit correspondiente
    size_t offset = (char*)blockAddr - (char*)memManager->base_address;
    size_t buddyOffset = offset ^ order_to_size(order);
    return (char*)memManager->base_address + buddyOffset;
}

// Elimina un nodo de su lista libre
static void removeFromFreeList(memory_manager_ADT memManager, buddy_node_t* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        // Es el primer nodo de la lista
        memManager->free_lists[index] = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
    
    node->next = NULL;
    node->prev = NULL;
}

// Agrega un nodo al inicio de su lista libre
static void addToFreeList(memory_manager_ADT memManager, buddy_node_t* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    node->next = memManager->free_lists[index];
    node->prev = NULL;
    node->order = order;
    node->free = true;
    
    if (memManager->free_lists[index]) {
        memManager->free_lists[index]->prev = node;
    }
    
    memManager->free_lists[index] = node;
}

// Divide un bloque en dos buddies
static buddy_node_t* splitBlock(memory_manager_ADT memManager, uint8_t order) {
    if (order == MIN_ORDER) {
        return NULL;  // No se puede dividir más
    }
    
    if (order > MAX_ORDER) {
        return NULL;  // Orden inválido
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques de este orden, dividir uno más grande
    if (memManager->free_lists[index] == NULL) {
        if (splitBlock(memManager, order + 1) == NULL) {
            return NULL;  // No hay bloques más grandes
        }
    }
    
    // Tomar el primer bloque de este orden
    buddy_node_t* block = memManager->free_lists[index];
    removeFromFreeList(memManager, block, order);
    
    // Dividir en dos buddies de orden-1
    uint8_t newOrder = order - 1;
    size_t halfSize = order_to_size(newOrder);
    
    // Primer buddy (el bloque original)
    buddy_node_t* buddy1 = block;
    
    // Segundo buddy (mitad superior)
    buddy_node_t* buddy2 = (buddy_node_t*)((char*)block + halfSize);
    
    // Agregar ambos a la lista del orden inferior
    addToFreeList(memManager, buddy1, newOrder);
    addToFreeList(memManager, buddy2, newOrder);
    
    return buddy1;
}

// Fusiona un bloque con su buddy si es posible
static void coalesce(memory_manager_ADT memManager, buddy_node_t* block) {
    uint8_t order = block->order;
    
    if (order >= MAX_ORDER) {
        return;  // Ya es el bloque más grande
    }
    
    // Buscar el buddy
    void* buddyAddr = getBuddyAddress(memManager, block, order);
    buddy_node_t* buddy = (buddy_node_t*)buddyAddr;
    
    // Verificar si el buddy está libre y tiene el mismo orden
    if (!buddy->free || buddy->order != order) {
        return;  // No se puede fusionar
    }
    
    // Eliminar ambos de sus listas
    removeFromFreeList(memManager, block, order);
    removeFromFreeList(memManager, buddy, order);
    
    // El bloque con dirección menor se convierte en el bloque fusionado
    buddy_node_t* merged = (block < buddy) ? block : buddy;
    
    // Agregar a la lista del siguiente orden
    addToFreeList(memManager, merged, order + 1);
    
    // Intentar fusionar recursivamente
    coalesce(memManager, merged);
}

// ============================================
//        IMPLEMENTACIÓN DE LA INTERFAZ
// ============================================

memory_manager_ADT createMemoryManager(void* startAddress, size_t size) {
    if (startAddress == NULL || size < sizeof(struct memory_manager_CDT) + (1ULL << MIN_ORDER)) {
        return NULL;
    }
    
    // Colocar el CDT al inicio
    memory_manager_ADT memManager = (memory_manager_ADT)startAddress;
    memManager->base_address = (char*)startAddress + sizeof(struct memory_manager_CDT);
    memManager->total_size = size - sizeof(struct memory_manager_CDT);
    memManager->allocated_blocks = 0;
    memManager->total_allocated = 0;
    
    // Inicializar listas libres
    for (int i = 0; i < NUM_ORDERS; i++) {
        memManager->free_lists[i] = NULL;
    }
    
    // Crear bloques iniciales con la memoria disponible
    size_t remainingSize = memManager->total_size;
    void* currentAddr = memManager->base_address;
    
    // Dividir la memoria en bloques del máximo orden posible
    while (remainingSize >= (1ULL << MIN_ORDER)) {
        uint8_t order = MAX_ORDER;
        size_t block_size = order_to_size(order);
        
        // Encontrar el bloque más grande que cabe
        while (block_size > remainingSize && order > MIN_ORDER) {
            order--;
            block_size = order_to_size(order);
        }
        
        // Crear el bloque
        buddy_node_t* node = (buddy_node_t*)currentAddr;
        addToFreeList(memManager, node, order);  // ← Ahora pasa memManager
        
        currentAddr = (char*)currentAddr + block_size;
        remainingSize -= block_size;
    }
    
    return memManager;
}

void* alloc_memory(memory_manager_ADT memManager, size_t size) {
    if (memManager == NULL || size == 0) {
        return NULL;
    }
    
    // Calcular el orden necesario
    uint8_t order = size_to_order(size);
    
    if (order > MAX_ORDER) {
        return NULL;  // Demasiado grande
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques del orden exacto, dividir uno más grande
    if (memManager->free_lists[index] == NULL) {
        if (splitBlock(memManager, order + 1) == NULL) {
            return NULL;  // Sin memoria
        }
    }
    
    // Tomar el primer bloque del orden
    buddy_node_t* block = memManager->free_lists[index];
    removeFromFreeList(memManager, block, order);
    
    // Marcar como ocupado
    block->free = false;
    block->order = order;
    
    memManager->allocated_blocks++;
    memManager->total_allocated += order_to_size(order) - sizeof(buddy_node_t);
    
    // Retornar puntero después del header
    return (char*)block + sizeof(buddy_node_t);
}

void free_memory(memory_manager_ADT memManager, void* ptr) {
    if (memManager == NULL || ptr == NULL) {
        return;
    }
    
    // Obtener el bloque
    buddy_node_t* block = (buddy_node_t*)((char*)ptr - sizeof(buddy_node_t));
    
    if (block->free) {
        return;  // Double free
    }
    
    // Marcar como libre
    block->free = true;
    memManager->allocated_blocks--;
    memManager->total_allocated -= order_to_size(block->order) - sizeof(buddy_node_t);
    
    // Agregar a la lista libre
    addToFreeList(memManager, block, block->order);
    
    // Intentar fusionar con el buddy
    coalesce(memManager, block);
}

mem_info_t get_mem_status(memory_manager_ADT memManager) {
    mem_info_t status = {0};
    
    if (memManager != NULL) {
        status.total_memory = memManager->total_size;
        status.used_memory = memManager->total_allocated;
        status.free_memory = status.total_memory - status.used_memory;
        status.allocated_blocks = memManager->allocated_blocks;
    }
    
    return status;
}

void printMemState(memory_manager_ADT memManager) {
    if (memManager == NULL) {
        ncPrint("Buddy Memory Manager: NULL\n");
        return;
    }
    
    ncPrint("=== BUDDY MEMORY STATE ===\n");
    ncPrint("Base Address: 0x");
    ncPrintHex((uint64_t)memManager->base_address);
    ncPrint("\n");
    
    ncPrint("Total Size: ");
    ncPrintDec(memManager->total_size / 1024);
    ncPrint(" KB\n");
    
    ncPrint("Allocated: ");
    ncPrintDec(memManager->total_allocated / 1024);
    ncPrint(" KB in ");
    ncPrintDec(memManager->allocated_blocks);
    ncPrint(" blocks\n");
    
    // Mostrar listas libres por orden
    ncPrint("\nFree Lists:\n");
    for (int i = 0; i < NUM_ORDERS; i++) {
        uint8_t order = MIN_ORDER + i;
        int count = 0;
        buddy_node_t* node = memManager->free_lists[i];
        
        while (node != NULL) {
            count++;
            node = node->next;
        }
        
        if (count > 0) {
            ncPrint("  Order ");
            ncPrintDec(order);
            ncPrint(" (");
            ncPrintDec(order_to_size(order) / 1024);
            ncPrint(" KB): ");
            ncPrintDec(count);
            ncPrint(" blocks\n");
        }
    }
    
    ncPrint("==========================\n");
}

void init_kernel_memory_manager(void) {
    kernel_mm = createMemoryManager((void*)HEAP_START_ADDRESS, HEAP_SIZE);
    
    if (kernel_mm == NULL) {
        ncPrint("KERNEL PANIC: Failed to initialize Buddy memory manager\n");
        while(1) {
            __asm__ __volatile__("hlt");
        }
    }
    
    ncPrint("Buddy Memory Manager initialized at 0x");
    ncPrintHex(HEAP_START_ADDRESS);
    ncPrint(" with ");
    ncPrintDec(HEAP_SIZE / (1024 * 1024));
    ncPrint(" MB\n");
    
    ncPrint("  Min block size: ");
    ncPrintDec(1 << MIN_ORDER);
    ncPrint(" bytes (order ");
    ncPrintDec(MIN_ORDER);
    ncPrint(")\n");
    
    ncPrint("  Max block size: ");
    ncPrintDec((1 << MAX_ORDER) / 1024);
    ncPrint(" KB (order ");
    ncPrintDec(MAX_ORDER);
    ncPrint(")\n");
}

memory_manager_ADT get_kernel_memory_manager(void) {
    return kernel_mm;
}

// ============================================
//         WRAPPERS PARA SYSCALLS
// ============================================

void* sys_malloc(size_t size) {
    return alloc_memory(kernel_mm, size);
}

void sys_free(void* ptr) {
    free_memory(kernel_mm, ptr);
}

mem_info_t sys_mem_info_t(void) {
    return get_mem_status(kernel_mm);
}