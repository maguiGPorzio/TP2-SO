#ifdef USE_BUDDY

#include "memoryManager.h"
#include <stdbool.h>
#include <stdint.h>
#include "naiveConsole.h"

#define MIN_ORDER 5        // 2^5 = 32 bytes (tamaño mínimo)
#define MAX_ORDER 20       // 2^20 = 1 MB (tamaño máximo de bloque)
#define NUM_ORDERS (MAX_ORDER - MIN_ORDER + 1)  // 16 niveles

// Nodo de la lista libre para cada orden
typedef struct BuddyNode {
    struct BuddyNode* next;  // Siguiente en la lista libre
    struct BuddyNode* prev;  // Anterior en la lista libre
    bool free;               // ¿Está libre?
    uint8_t order;          // Orden del bloque (2^order bytes)
} BuddyNode;

// Estructura del Buddy Memory Manager
struct MemoryManagerCDT {
    void* baseAddress;                    // Dirección base de la memoria
    size_t totalSize;                     // Tamaño total
    BuddyNode* freeLists[NUM_ORDERS];    // Array de listas libres por orden
    size_t allocatedBlocks;               // Bloques allocados
    size_t totalAllocated;                // Bytes totales allocados
};

// ============================================
//         VARIABLES GLOBALES
// ============================================

static MemoryManagerADT kernelMemManager = NULL;

// ============================================
//          FUNCIONES AUXILIARES
// ============================================

// Calcula el tamaño de un bloque dado su orden
static size_t orderToSize(uint8_t order) {
    return (1ULL << order);
}

// Calcula el orden necesario para un tamaño dado
static uint8_t sizeToOrder(size_t size) {
    // Incluir espacio para el header
    size += sizeof(BuddyNode);
    
    uint8_t order = MIN_ORDER;
    size_t blockSize = orderToSize(order); //ESTO SERIA 32 PORQUE MIN_ORDER ES 5
    
    while (blockSize < size && order < MAX_ORDER) {
        order++;
        blockSize = orderToSize(order);
    }
    
    return order;
}

// Calcula la dirección del buddy de un bloque
static void* getBuddyAddress(void* blockAddr, uint8_t order) {
    // XOR con el tamaño del bloque para flip el bit correspondiente
    size_t offset = (char*)blockAddr - (char*)kernelMemManager->baseAddress;
    size_t buddyOffset = offset ^ orderToSize(order);
    return (char*)kernelMemManager->baseAddress + buddyOffset;
}

// Verifica si dos bloques son buddies
static bool areBuddies(void* addr1, void* addr2, uint8_t order) {
    return getBuddyAddress(addr1, order) == addr2;
}

// Elimina un nodo de su lista libre
static void removeFromFreeList(BuddyNode* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        // Es el primer nodo de la lista
        kernelMemManager->freeLists[index] = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
    
    node->next = NULL;
    node->prev = NULL;
}

// Agrega un nodo al inicio de su lista libre
static void addToFreeList(BuddyNode* node, uint8_t order) {
    uint8_t index = order - MIN_ORDER;
    
    node->next = kernelMemManager->freeLists[index];
    node->prev = NULL;
    node->order = order;
    node->free = true;
    
    if (kernelMemManager->freeLists[index]) {
        kernelMemManager->freeLists[index]->prev = node;
    }
    
    kernelMemManager->freeLists[index] = node;
}

// Divide un bloque en dos buddies
static BuddyNode* splitBlock(uint8_t order) {
    if (order == MIN_ORDER) {
        return NULL;  // No se puede dividir más
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques de este orden, dividir uno más grande
    if (kernelMemManager->freeLists[index] == NULL) {
        if (splitBlock(order + 1) == NULL) {
            return NULL;  // No hay bloques más grandes
        }
    }
    
    // Tomar el primer bloque de este orden
    BuddyNode* block = kernelMemManager->freeLists[index];
    removeFromFreeList(block, order);
    
    // Dividir en dos buddies de orden-1
    uint8_t newOrder = order - 1;
    size_t halfSize = orderToSize(newOrder);
    
    // Primer buddy (el bloque original)
    BuddyNode* buddy1 = block;
    
    // Segundo buddy (mitad superior)
    BuddyNode* buddy2 = (BuddyNode*)((char*)block + halfSize);
    
    // Agregar ambos a la lista del orden inferior
    addToFreeList(buddy1, newOrder);
    addToFreeList(buddy2, newOrder);
    
    return buddy1;
}

// Fusiona un bloque con su buddy si es posible
static void coalesce(BuddyNode* block) {
    uint8_t order = block->order;
    
    if (order >= MAX_ORDER) {
        return;  // Ya es el bloque más grande
    }
    
    // Buscar el buddy
    void* buddyAddr = getBuddyAddress(block, order);
    BuddyNode* buddy = (BuddyNode*)buddyAddr;
    
    // Verificar si el buddy está libre y tiene el mismo orden
    if (!buddy->free || buddy->order != order) {
        return;  // No se puede fusionar
    }
    
    // Eliminar ambos de sus listas
    removeFromFreeList(block, order);
    removeFromFreeList(buddy, order);
    
    // El bloque con dirección menor se convierte en el bloque fusionado
    BuddyNode* merged = (block < buddy) ? block : buddy;
    
    // Agregar a la lista del siguiente orden
    addToFreeList(merged, order + 1);
    
    // Intentar fusionar recursivamente
    coalesce(merged);
}

// ============================================
//        IMPLEMENTACIÓN DE LA INTERFAZ
// ============================================

MemoryManagerADT createMemoryManager(void* startAddress, size_t size) {
    if (startAddress == NULL || size < sizeof(struct MemoryManagerCDT) + (1ULL << MIN_ORDER)) {
        return NULL;
    }
    
    // Colocar el CDT al inicio
    MemoryManagerADT memManager = (MemoryManagerADT)startAddress;
    memManager->baseAddress = (char*)startAddress + sizeof(struct MemoryManagerCDT);
    memManager->totalSize = size - sizeof(struct MemoryManagerCDT);
    memManager->allocatedBlocks = 0;
    memManager->totalAllocated = 0;
    
    // Inicializar listas libres
    for (int i = 0; i < NUM_ORDERS; i++) {
        memManager->freeLists[i] = NULL;
    }
    
    // Crear bloques iniciales con la memoria disponible
    size_t remainingSize = memManager->totalSize;
    void* currentAddr = memManager->baseAddress;
    
    // Dividir la memoria en bloques del máximo orden posible
    while (remainingSize >= (1ULL << MIN_ORDER)) {
        uint8_t order = MAX_ORDER;
        size_t blockSize = orderToSize(order);
        
        // Encontrar el bloque más grande que cabe
        while (blockSize > remainingSize && order > MIN_ORDER) {
            order--;
            blockSize = orderToSize(order);
        }
        
        // Crear el bloque
        BuddyNode* node = (BuddyNode*)currentAddr;
        addToFreeList(node, order);
        
        currentAddr = (char*)currentAddr + blockSize;
        remainingSize -= blockSize;
    }
    
    return memManager;
}

void* allocMemory(MemoryManagerADT memManager, size_t size) {
    if (memManager == NULL || size == 0) {
        return NULL;
    }
    
    // Calcular el orden necesario
    uint8_t order = sizeToOrder(size);
    
    if (order > MAX_ORDER) {
        return NULL;  // Demasiado grande
    }
    
    uint8_t index = order - MIN_ORDER;
    
    // Si no hay bloques del orden exacto, dividir uno más grande
    if (memManager->freeLists[index] == NULL) {
        if (splitBlock(order + 1) == NULL) {
            return NULL;  // Sin memoria
        }
    }
    
    // Tomar el primer bloque del orden
    BuddyNode* block = memManager->freeLists[index];
    removeFromFreeList(block, order);
    
    // Marcar como ocupado
    block->free = false;
    block->order = order;
    
    memManager->allocatedBlocks++;
    memManager->totalAllocated += orderToSize(order) - sizeof(BuddyNode);
    
    // Retornar puntero después del header
    return (char*)block + sizeof(BuddyNode);
}

void freeMemory(MemoryManagerADT memManager, void* ptr) {
    if (memManager == NULL || ptr == NULL) {
        return;
    }
    
    // Obtener el bloque
    BuddyNode* block = (BuddyNode*)((char*)ptr - sizeof(BuddyNode));
    
    if (block->free) {
        return;  // Double free
    }
    
    // Marcar como libre
    block->free = true;
    memManager->allocatedBlocks--;
    memManager->totalAllocated -= orderToSize(block->order) - sizeof(BuddyNode);
    
    // Agregar a la lista libre
    addToFreeList(block, block->order);
    
    // Intentar fusionar con el buddy
    coalesce(block);
}

MemStatus getMemStatus(MemoryManagerADT memManager) {
    MemStatus status = {0};
    
    if (memManager != NULL) {
        status.totalMemory = memManager->totalSize;
        status.usedMemory = memManager->totalAllocated;
        status.freeMemory = status.totalMemory - status.usedMemory;
        status.allocatedBlocks = memManager->allocatedBlocks;
    }
    
    return status;
}

void initKernelMemoryManager(void) {
    kernelMemManager = createMemoryManager((void*)HEAP_START_ADDRESS, HEAP_SIZE);
    
    if (kernelMemManager == NULL) {
        // Kernel panic - no se pudo inicializar el gestor
        ncPrint("KERNEL PANIC: Failed to initialize Buddy memory manager\n");
        while(1); // Halt - detener el sistema
    }
    
    ncPrint("Buddy Memory Manager initialized at 0x");
    ncPrintHex(HEAP_START_ADDRESS);
    ncPrint(" with ");
    ncPrintDec(HEAP_SIZE / (1024 * 1024));
    ncPrint(" MB\n");
    
    // Imprimir información sobre los órdenes disponibles
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

#endif // USE_BUDDY