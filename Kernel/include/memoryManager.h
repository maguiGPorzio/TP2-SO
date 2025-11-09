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

// TAD - Tipo ABStracto de datos (estructura opaca)
typedef struct memory_manager_CDT* memory_manager_ADT;

// Estructura para estadísticas de memoria
typedef struct {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    size_t allocated_blocks;
} mem_info_t;

/**
 * @brief Reserva un bloque de memoria
 * @param memory_manager Handler del memory manager
 * @param size Tamaño en bytes a reservar
 * @return Puntero al bloque reservado o NULL si no hay memoria
 */
void* alloc_memory(memory_manager_ADT memory_manager, size_t size);

/**
 * @brief Libera un bloque de memoria previamente reservado
 * @param memory_manager Handler del memory manager
 * @param ptr Puntero al bloque a liberar
 */
void free_memory(memory_manager_ADT memory_manager, void* ptr);

/**
 * @brief Obtiene estadísticas del estado de la memoria
 * @param memory_manager Handler del memory manager
 * @return Estructura con información de memoria
 */
mem_info_t get_mem_status(memory_manager_ADT memory_manager);


/**
 * @brief Inicializa el memory manager del kernel
 * Se llama una sola vez durante el boot
 */
void init_kernel_memory_manager(void);

memory_manager_ADT get_kernel_memory_manager(void);

#endif // MEMORY_MANAGER_ADT_H