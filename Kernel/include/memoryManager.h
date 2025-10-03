#ifndef MEMORY_MANAGER_ADT_H
#define MEMORY_MANAGER_ADT_H

#include <stdint.h>
#include <stddef.h>

// ============================================
//           CONSTANTES Y CONFIGURACIÓN
// ============================================

#define HEAP_START_ADDRESS 0x600000    // Dirección de inicio del heap
#define HEAP_SIZE 0x2000000             // 32MB de heap

// ============================================
//              TIPOS DE DATOS
// ============================================

// TAD - Tipo abstracto de datos (estructura opaca)
typedef struct MemoryManagerCDT* MemoryManagerADT;

// Estructura para estadísticas de memoria
typedef struct {
    size_t totalMemory;
    size_t usedMemory;
    size_t freeMemory;
    size_t allocatedBlocks;
} MemStatus;

// ============================================
//           INTERFAZ PÚBLICA DEL TAD
// ============================================

/**
 * @brief Crea e inicializa un nuevo memory manager
 * @param startAddress Dirección de inicio de la memoria a administrar
 * @param size Tamaño total de la memoria en bytes
 * @return Handler del memory manager o NULL si falla
 */
MemoryManagerADT createMemoryManager(void* startAddress, size_t size);

/**
 * @brief Reserva un bloque de memoria
 * @param memManager Handler del memory manager
 * @param size Tamaño en bytes a reservar
 * @return Puntero al bloque reservado o NULL si no hay memoria
 */
void* allocMemory(MemoryManagerADT memManager, size_t size);

/**
 * @brief Libera un bloque de memoria previamente reservado
 * @param memManager Handler del memory manager
 * @param ptr Puntero al bloque a liberar
 */
void freeMemory(MemoryManagerADT memManager, void* ptr);

/**
 * @brief Obtiene estadísticas del estado de la memoria
 * @param memManager Handler del memory manager
 * @return Estructura con información de memoria
 */
MemStatus getMemStatus(MemoryManagerADT memManager);

/**
 * @brief Imprime información de debug sobre el estado de la memoria
 * @param memManager Handler del memory manager
 */
void printMemState(MemoryManagerADT memManager);

// ============================================
//     FUNCIONES GLOBALES PARA EL KERNEL
// ============================================

/**
 * @brief Inicializa el memory manager del kernel
 * Se llama una sola vez durante el boot
 */
void initKernelMemoryManager(void);

/**
 * @brief Obtiene el memory manager del kernel
 * @return Handler del memory manager del kernel
 */
MemoryManagerADT getKernelMemoryManager(void);

// ============================================
//         WRAPPERS PARA SYSCALLS
// ============================================

void* sys_malloc(size_t size);
void sys_free(void* ptr);
MemStatus sys_memStatus(void);

#endif // MEMORY_MANAGER_ADT_H