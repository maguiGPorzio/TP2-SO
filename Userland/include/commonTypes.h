#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t totalMemory;
    size_t usedMemory;
    size_t freeMemory;
    size_t allocatedBlocks;
} MemStatus;