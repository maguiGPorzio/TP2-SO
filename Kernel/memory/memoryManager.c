// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "memory_manager.h"
#include <string.h>
#include <stdbool.h>
#include "naiveConsole.h"

#define MIN_BLOCK_SIZE 32       // Tamaño mínimo de bloque
#define ALIGN_SIZE 8            // Alineación de memoria (8 bytes)
#define MAGIC_NUMBER 0xDEADBEEF // Para detectar corrupción

// Header de cada bloque de memoria. Es una lista doblemente enlazada.
typedef struct mem_block {
	size_t            size; // Tamaño del bloque (sin incluir header)
	struct mem_block *next; // Siguiente bloque en la lista
	struct mem_block *prev; // Bloque anterior
	bool              free; // true si está libre, false si está ocupado
	uint32_t magic; // Número mágico para verificación.  Al liberar (free_memory) se verifica
	                // que block->magic == MAGIC_NUMBER antes de confiar en el puntero; si
	                // alguien pasó una dirección que no proviene del gestor (o fue
	                // sobrescrita), la comparación falla y se ignora la operación
} mem_block;

// Estructura del Memory Manager (CDT)
struct memory_manager_CDT {
	void      *start_address;    // Dirección base de la memoria
	size_t     total_size;       // Tamaño total
	mem_block *first_block;      // Primer bloque de la lista
	size_t     allocated_blocks; // Contador de bloques allocados
	size_t     total_allocated;  // Total de bytes allocados
};

static memory_manager_ADT kernel_mm = NULL;

// Alinea un tamaño al múltiplo de ALIGN_SIZE
static size_t align(size_t size)
{
	return (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

// Divide un bloque si es muy grande
static void split_block(mem_block *block, size_t size)
{
	if (block->size >= size + sizeof(mem_block) + MIN_BLOCK_SIZE) {
		// Crear nuevo bloque con el espacio restante
		mem_block *newBlock = (mem_block *)((char *)block + sizeof(mem_block) + size);
		newBlock->size      = block->size - size - sizeof(mem_block);
		newBlock->free      = true; // ACA
		newBlock->magic     = MAGIC_NUMBER;
		newBlock->next      = block->next;
		newBlock->prev      = block;

		if (block->next != NULL) {
			block->next->prev = newBlock;
		}

		block->next = newBlock;
		block->size = size;
	}
}

static inline void coalesce_next(mem_block *block)
{
	if (block == NULL) {
		return;
	}
	mem_block *next = block->next;
	if (next != NULL && next->free) {
		block->size += sizeof(mem_block) + next->size;
		block->next = next->next;
		if (block->next != NULL) {
			block->next->prev = block;
		}
	}
}

static inline void coalesce_prev(mem_block *block)
{
	if (block == NULL)
		return;
	mem_block *prev = block->prev;
	if (prev != NULL && prev->free) {
		prev->size += sizeof(mem_block) + block->size;
		prev->next = block->next;
		if (block->next != NULL) {
			block->next->prev = prev;
		}
	}
}

// Fusiona bloques libres adyacentes usando funciones auxiliares. TIENEN QUE ESTAR EN ESE ORDEN!
static void coalesce_blocks(mem_block *block)
{
	coalesce_next(block);
	coalesce_prev(block);
}

// Busca el primer bloque libre que tenga el tamaño suficiente (First Fit)
static mem_block *find_free_block(memory_manager_ADT memory_manager, size_t size)
{
	mem_block *current = memory_manager->first_block;

	while (current != NULL) {
		if (current->free && current->size >= size && current->magic == MAGIC_NUMBER) {
			return current;
		}
		current = current->next;
	}

	return NULL; // No hay bloque libre suficiente
}

memory_manager_ADT create_memory_manager(void *start_address, size_t size)
{
	// Valida tamaño mínimo para el memory manager y al menos un bloque
	if (start_address == NULL ||
	    size < sizeof(struct memory_manager_CDT) + sizeof(mem_block) + MIN_BLOCK_SIZE) {
		return NULL;
	}

	// El memory manager se almacena al inicio del área de memoria
	memory_manager_ADT memory_manager = (memory_manager_ADT)start_address;
	// El gestor guarda su propia estructura directamente en la memoria gestionada: el puntero
	// apunta al inicio del heap y se interpreta como la estructura para poder escribir sus
	// campos.

	// Inicializar estructura
	memory_manager->start_address    = start_address;
	memory_manager->total_size       = size;
	memory_manager->allocated_blocks = 0;
	memory_manager->total_allocated  = 0;

	// Crear el primer bloque libre después del CDT
	memory_manager->first_block =
	        (mem_block *)((char *)start_address + sizeof(struct memory_manager_CDT));
	memory_manager->first_block->size =
	        size - sizeof(struct memory_manager_CDT) -
	        sizeof(mem_block); // Resto del espacio del memory manager
	memory_manager->first_block->free  = true;
	memory_manager->first_block->next  = NULL;
	memory_manager->first_block->prev  = NULL;
	memory_manager->first_block->magic = MAGIC_NUMBER;

	return memory_manager;
}

void *alloc_memory(memory_manager_ADT memory_manager, size_t size)
{
	if (memory_manager == NULL || size == 0) {
		return NULL;
	}

	// Alinear el tamaño
	size = align(size);

	// Buscar un bloque libre
	mem_block *block = find_free_block(memory_manager, size);

	if (block == NULL) {
		return NULL; // No hay memoria disponible
	}

	// Dividir el bloque si es necesario
	split_block(block, size);

	// Marcar como ocupado
	block->free = false;
	memory_manager->allocated_blocks++;
	memory_manager->total_allocated += block->size;

	// Retornar puntero después del header (donde arranca el espacio utilizable del bloque)
	return (char *)block + sizeof(mem_block);
}

void free_memory(memory_manager_ADT memory_manager, void *ptr)
{
	if (memory_manager == NULL || ptr == NULL) {
		return;
	}

	// Obtener el bloque desde el puntero
	mem_block *block = (mem_block *)((char *)ptr - sizeof(mem_block));

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
	memory_manager->allocated_blocks--;
	memory_manager->total_allocated -= block->size;

	// Fusionar con bloques adyacentes
	coalesce_blocks(block);
}

mem_info_t get_mem_status(memory_manager_ADT memory_manager)
{
	mem_info_t status = {0};

	if (memory_manager != NULL) {
		status.total_memory = memory_manager->total_size;
		status.used_memory  = memory_manager->total_allocated;
		status.free_memory  = status.total_memory - status.used_memory -
		                     sizeof(struct memory_manager_CDT) -
		                     (memory_manager->allocated_blocks * sizeof(mem_block));
		status.allocated_blocks = memory_manager->allocated_blocks;
	}

	return status;
}

void init_kernel_memory_manager(void)
{
	kernel_mm = create_memory_manager((void *)HEAP_START_ADDRESS, HEAP_SIZE);
}

memory_manager_ADT get_kernel_memory_manager(void)
{
	return kernel_mm;
}