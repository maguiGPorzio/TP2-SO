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
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    size_t allocated_blocks;
} mem_info_t;

/**
 * @brief Reserva un bloque de memoria
 * @param memManager Handler del memory manager
 * @param size Tamaño en bytes a reservar
 * @return Puntero al bloque reservado o NULL si no hay memoria
 */
void* alloc_memory(MemoryManagerADT memManager, size_t size);

/**
 * @brief Libera un bloque de memoria previamente reservado
 * @param memManager Handler del memory manager
 * @param ptr Puntero al bloque a liberar
 */
void free_memory(MemoryManagerADT memManager, void* ptr);

/**
 * @brief Obtiene estadísticas del estado de la memoria
 * @param memManager Handler del memory manager
 * @return Estructura con información de memoria
 */
mem_info_t get_mem_status(MemoryManagerADT memManager);


/**
 * @brief Inicializa el memory manager del kernel
 * Se llama una sola vez durante el boot
 */
void init_kernel_memory_manager(void);

MemoryManagerADT get_kernel_memory_manager(void);

#endif // MEMORY_MANAGER_ADT_H