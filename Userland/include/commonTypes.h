#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    size_t allocated_blocks;
} MemStatus;

typedef int (*process_entry_t)(int argc, char **argv);