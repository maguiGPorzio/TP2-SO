// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "memoryManager.h"
#include <string.h>
#include <stdbool.h>
#include "naiveConsole.h"

#define MIN_BLOCK_SIZE 32       // Tamaño mínimo de bloque
#define ALIGN_SIZE 8            // Alineación de memoria (8 bytes)
#define MAGIC_NUMBER 0xDEADBEEF // Para detectar corrupción

// ============================================
//           ESTRUCTURAS PRIVADAS
// ============================================

// Header de cada bloque de memoria. Es una lista doblemente enlazada.
typedef struct MemBlock {
    size_t size;                // Tamaño del bloque (sin incluir header)
    struct MemBlock* next;      // Siguiente bloque en la lista
    struct MemBlock* prev;      // Bloque anterior
    bool free;                  // true si está libre, false si está ocupado
    uint32_t magic;             // Número mágico para verificación.  Al liberar (free_memory) se verifica que block->magic == MAGIC_NUMBER antes de confiar en el puntero; si alguien pasó una dirección que no proviene del gestor (o fue sobrescrita), la comparación falla y se ignora la operación
} MemBlock;

// Estructura del Memory Manager (CDT)
struct MemoryManagerCDT {
    void* startAddress;         // Dirección base de la memoria
    size_t totalSize;           // Tamaño total
    MemBlock* firstBlock;       // Primer bloque de la lista
    size_t allocated_blocks;     // Contador de bloques allocados
    size_t totalAllocated;      // Total de bytes allocados
};

// ============================================
//         VARIABLES GLOBALES DEL KERNEL
// ============================================

static MemoryManagerADT kernelMemManager = NULL;

// ============================================
//          FUNCIONES AUXILIARES PRIVADAS
// ============================================

// Alinea un tamaño al múltiplo de ALIGN_SIZE
static size_t align(size_t size) {
    return (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

// Divide un bloque si es muy grande
static void splitBlock(MemBlock* block, size_t size) {
    
    if (block->size >= size + sizeof(MemBlock) + MIN_BLOCK_SIZE) {
        // Crear nuevo bloque con el espacio restante
        MemBlock* newBlock = (MemBlock*)((char*)block + sizeof(MemBlock) + size);
        newBlock->size = block->size - size - sizeof(MemBlock);
        newBlock->free = true; // ACA
        newBlock->magic = MAGIC_NUMBER;
        newBlock->next = block->next;
        newBlock->prev = block;
        
        if (block->next != NULL) {
            block->next->prev = newBlock;
        }
        
        block->next = newBlock;
        block->size = size;
    }
}

static inline void coalesceNext(MemBlock* block) {
    if (block == NULL) return;
    MemBlock* next = block->next;
    if (next != NULL && next->free) {
        block->size += sizeof(MemBlock) + next->size;
        block->next = next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }
}

static inline void coalescePrev(MemBlock* block) {
    if (block == NULL) return;
    MemBlock* prev = block->prev;
    if (prev != NULL && prev->free) {
        prev->size += sizeof(MemBlock) + block->size;
        prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = prev;
        }
    }
}

// Fusiona bloques libres adyacentes usando funciones auxiliares. TIENEN QUE ESTAR EN ESE ORDEN!
static void coalesceBlocks(MemBlock* block) {
    coalesceNext(block);
    coalescePrev(block);
}

// Busca el primer bloque libre que tenga el tamaño suficiente (First Fit)
static MemBlock* findFreeBlock(MemoryManagerADT memManager, size_t size) {
    MemBlock* current = memManager->firstBlock;
    
    while (current != NULL) {
        if (current->free && current->size >= size && current->magic == MAGIC_NUMBER) {
            return current;
        }
        current = current->next;
    }
    
    return NULL; // No hay bloque libre suficiente
}

// ============================================
//        IMPLEMENTACIÓN DE LA INTERFAZ
// ============================================

MemoryManagerADT createMemoryManager(void* startAddress, size_t size) {
    // Valida tamaño mínimo para el memory manager y al menos un bloque
    if (startAddress == NULL || size < sizeof(struct MemoryManagerCDT) + sizeof(MemBlock) + MIN_BLOCK_SIZE) {
        return NULL;
    }
    
    // El memory manager se almacena al inicio del área de memoria
    MemoryManagerADT memManager = (MemoryManagerADT)startAddress;
    // El gestor guarda su propia estructura directamente en la memoria gestionada: el puntero apunta al inicio del heap y se interpreta como la estructura para poder escribir sus campos.
    
    // Inicializar estructura
    memManager->startAddress = startAddress;
    memManager->totalSize = size;
    memManager->allocated_blocks = 0;
    memManager->totalAllocated = 0;
    
    // Crear el primer bloque libre después del CDT
    memManager->firstBlock = (MemBlock*)((char*)startAddress + sizeof(struct MemoryManagerCDT));
    memManager->firstBlock->size = size - sizeof(struct MemoryManagerCDT) - sizeof(MemBlock); // Resto del espacio del memory manager
    memManager->firstBlock->free = true;
    memManager->firstBlock->next = NULL;
    memManager->firstBlock->prev = NULL;
    memManager->firstBlock->magic = MAGIC_NUMBER;
    
    return memManager;
}

void* alloc_memory(MemoryManagerADT memManager, size_t size) {
    if (memManager == NULL || size == 0) {
        return NULL;
    }
    
    // Alinear el tamaño
    size = align(size);
    
    // Buscar un bloque libre
    MemBlock* block = findFreeBlock(memManager, size);
    
    if (block == NULL) {
        return NULL; // No hay memoria disponible
    }
    
    // Dividir el bloque si es necesario
    splitBlock(block, size);
    
    // Marcar como ocupado
    block->free = false;
    memManager->allocated_blocks++;
    memManager->totalAllocated += block->size;
    
    // Retornar puntero después del header (donde arranca el espacio utilizable del bloque)
    return (char*)block + sizeof(MemBlock);
}

void free_memory(MemoryManagerADT memManager, void* ptr) {
    if (memManager == NULL || ptr == NULL) {
        return;
    }
    
    // Obtener el bloque desde el puntero
    MemBlock* block = (MemBlock*)((char*)ptr - sizeof(MemBlock));
    
    // Verificar número mágico
    if (block->magic != MAGIC_NUMBER) {
        // Memoria corrupta o puntero inválido
        return;
    }
    
    // Verificar que no esté ya libre (double free)
    if (block->free) {
        return;
    }
    
    // Marcar como libre
    block->free = true;
    memManager->allocated_blocks--;
    memManager->totalAllocated -= block->size;

    // Fusionar con bloques adyacentes
    coalesceBlocks(block);
}

mem_info_t get_mem_status(MemoryManagerADT memManager) {
    mem_info_t status = {0};
    
    if (memManager != NULL) {
        status.total_memory = memManager->totalSize;
        status.used_memory = memManager->totalAllocated;
        status.free_memory = status.total_memory - status.used_memory - 
                           sizeof(struct MemoryManagerCDT) - 
                           (memManager->allocated_blocks * sizeof(MemBlock));
        status.allocated_blocks = memManager->allocated_blocks;
    }
    
    return status;
}

void init_kernel_memory_manager(void) {
    kernelMemManager = createMemoryManager((void*)HEAP_START_ADDRESS, HEAP_SIZE);
    
    if (kernelMemManager == NULL) {
        // Kernel panic
        ncPrint("KERNEL PANIC: Failed to initialize memory manager\n");
        while(1); // Halt
    }
    
    ncPrint("Memory Manager initialized at 0x");
    ncPrintHex(HEAP_START_ADDRESS);
    ncPrint(" with ");
    ncPrintDec(HEAP_SIZE / (1024 * 1024));
    ncPrint(" MB\n");
}

MemoryManagerADT get_kernel_memory_manager(void) {
    return kernelMemManager;
}