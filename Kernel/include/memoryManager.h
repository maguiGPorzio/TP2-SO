#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

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
 * @brief Inicializa el memory manager del kernel
 * Se llama una sola vez durante el boot
 */
void initKernelMemoryManager(void);


#endif // MEMORY_MANAGER_ADT_H