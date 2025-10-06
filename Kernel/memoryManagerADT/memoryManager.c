// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "../include/memoryManager.h"
#include <string.h>
#include <stdbool.h>
#include "naiveConsole.h"

// ============================================
//           CONFIGURACIÓN INTERNA
// ============================================

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
    uint32_t magic;             // Número mágico para verificación.  Al liberar (freeMemory) se verifica que block->magic == MAGIC_NUMBER antes de confiar en el puntero; si alguien pasó una dirección que no proviene del gestor (o fue sobrescrita), la comparación falla y se ignora la operación
} MemBlock;

// Estructura del Memory Manager (CDT)
struct MemoryManagerCDT {
    void* startAddress;         // Dirección base de la memoria
    size_t totalSize;           // Tamaño total
    MemBlock* firstBlock;       // Primer bloque de la lista
    size_t allocatedBlocks;     // Contador de bloques allocados
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
    // Solo dividir si queda espacio suficiente para otro bloque
    size_t totalSize = sizeof(MemBlock) + size;
    
    if (block->size >= totalSize + sizeof(MemBlock) + MIN_BLOCK_SIZE) {
        // Crear nuevo bloque con el espacio restante
        MemBlock* newBlock = (MemBlock*)((char*)block + sizeof(MemBlock) + size);
        newBlock->size = block->size - size - sizeof(MemBlock);
        newBlock->free = true;
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

// Fusiona bloques libres adyacentes usando funciones auxiliares
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
    memManager->allocatedBlocks = 0;
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

void* allocMemory(MemoryManagerADT memManager, size_t size) {
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
    memManager->allocatedBlocks++;
    memManager->totalAllocated += block->size;
    
    // Retornar puntero después del header (donde arranca el espacio utilizable del bloque)
    return (char*)block + sizeof(MemBlock);
}

void freeMemory(MemoryManagerADT memManager, void* ptr) {
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
    memManager->allocatedBlocks--;
    memManager->totalAllocated -= block->size;

    // Fusionar con bloques adyacentes
    coalesceBlocks(block);
}

MemStatus getMemStatus(MemoryManagerADT memManager) {
    MemStatus status = {0};
    
    if (memManager != NULL) {
        status.totalMemory = memManager->totalSize;
        status.usedMemory = memManager->totalAllocated;
        status.freeMemory = status.totalMemory - status.usedMemory - 
                           sizeof(struct MemoryManagerCDT) - 
                           (memManager->allocatedBlocks * sizeof(MemBlock));
        status.allocatedBlocks = memManager->allocatedBlocks;
    }
    
    return status;
}

void printMemState(MemoryManagerADT memManager) {
    if (memManager == NULL) {
        ncPrint("Memory Manager: NULL\n");
        return;
    }
    
    ncPrint("=== MEMORY STATE ===\n");
    ncPrint("Base Address: 0x");
    ncPrintHex(memManager->startAddress);
    ncPrint("\n");

    ncPrint("Total Size:");
    ncPrintDec(memManager->totalSize / 1024);
    ncPrint(" KB\n");


    ncPrint("Allocated: ");
    ncPrintDec(memManager->totalAllocated / 1024);
    ncPrint(" KB in ");
    ncPrintDec(memManager->allocatedBlocks);
    ncPrint(" blocks\n");

    
    // Contar bloques libres y fragmentación
    int freeBlocks = 0;
    size_t largestFree = 0;
    size_t totalFree = 0;
    
    MemBlock* current = memManager->firstBlock;
    ncPrint("\nBlock List:\n");
    int i = 0;
    
    while (current != NULL && i < 10) { // Mostrar máximo 10 bloques
        if (current->free) {
            freeBlocks++;
            totalFree += current->size;
            if (current->size > largestFree) {
                largestFree = current->size;
            }
            ncPrint("  [");
            ncPrintDec(i);
            ncPrint("] FREE - Size: ");
            ncPrintDec(current->size);
            ncPrint(" bytes\n");
            
        } else {
            ncPrint("  [");
            ncPrintDec(i);
            ncPrint("] USED - Size: ");
            ncPrintDec(current->size);
            ncPrint(" bytes\n");
        }

        current = current->next;
        i++;
    }
    
    //print("\nFree Memory: %d KB in %d blocks\n", totalFree / 1024, freeBlocks);
    //print("Largest Free Block: %d KB\n", largestFree / 1024);
    //print("Fragmentation: %d blocks\n", freeBlocks);
    //print("====================\n");
    ncPrint("\nFree Memory: ");
    ncPrintDec(totalFree / 1024);
    ncPrint(" KB in ");
    ncPrintDec(freeBlocks);
    ncPrint(" blocks\n");
    ncPrint("Largest Free Block: ");
    ncPrintDec(largestFree / 1024);
    ncPrint(" KB\n");
    ncPrint("Fragmentation: ");
    ncPrintDec(freeBlocks);
    ncPrint(" blocks\n");
    ncPrint("====================\n");
}

// ============================================
//      FUNCIONES GLOBALES DEL KERNEL
// ============================================

void initKernelMemoryManager(void) {
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

MemoryManagerADT getKernelMemoryManager(void) {
    return kernelMemManager;
}

// ============================================
//         WRAPPERS PARA SYSCALLS
// ============================================

void* sys_malloc(size_t size) {
    return allocMemory(kernelMemManager, size);
}

void sys_free(void* ptr) {
    freeMemory(kernelMemManager, ptr);
}

MemStatus sys_memStatus(void) {
    return getMemStatus(kernelMemManager);
}
