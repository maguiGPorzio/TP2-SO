// #include "../include/test_mm.h" // para sys_* y MemStatus

// #define MAX_BLOCKS 128

// typedef struct MM_rq {
//   void *address;
//   uint32_t size;
// } mm_rq;

// static uint32_t m_z = 362436069;
// static uint32_t m_w = 521288629;

// uint32_t GetUint() {
//   m_z = 36969 * (m_z & 65535) + (m_z >> 16);
//   m_w = 18000 * (m_w & 65535) + (m_w >> 16);
//   return (m_z << 16) + m_w;
// }

// uint32_t GetUniform(uint32_t max) {
//   uint32_t u = GetUint();
//   return (u + 1.0) * 2.328306435454494e-10 * max;
// }

// int64_t satoi(char *str) {
//   uint64_t i = 0;
//   int64_t res = 0;
//   int8_t sign = 1;

//   if (!str)
//     return 0;

//   if (str[i] == '-') {
//     i++;
//     sign = -1;
//   }

//   for (; str[i] != '\0'; ++i) {
//     if (str[i] < '0' || str[i] > '9')
//       return 0;
//     res = res * 10 + str[i] - '0';
//   }

//   return res * sign;
// }

// uint8_t memcheck(void *start, uint8_t value, uint32_t size) {
//   uint8_t *p = (uint8_t *)start;
//   uint32_t i;

//   for (i = 0; i < size; i++, p++)
//     if (*p != value)
//       return 0;

//   return 1;
// }


// int64_t test_mm(uint64_t argc, char * argv[]){

//   mm_rq mm_rqs[MAX_BLOCKS];
//   uint8_t rq;
//   uint32_t total;
//   uint64_t max_memory;

//   if (argc != 1)
//     return -1;

//   if ((max_memory = satoi(argv[0])) <= 0)
//     return -1;

//   while (1) {
//     rq = 0;
//     total = 0;

//     // Request as many blocks as we can
//     while (rq < MAX_BLOCKS && total < max_memory) {
//       mm_rqs[rq].size = GetUniform(max_memory - total - 1) + 1;
//   mm_rqs[rq].address = sys_malloc(mm_rqs[rq].size);

//       if (mm_rqs[rq].address) {
//         total += mm_rqs[rq].size;
//         rq++;
//       }
//     }

//     // Set
//     uint32_t i;
//     for (i = 0; i < rq; i++)
//       if (mm_rqs[i].address)
//         memset(mm_rqs[i].address, i, mm_rqs[i].size);

//     // Check
//     for (i = 0; i < rq; i++)
//       if (mm_rqs[i].address)
//         if (!memcheck(mm_rqs[i].address, i, mm_rqs[i].size)) {
//           printf("test_mm ERROR\n");
//           return -1;
//         }

//     // Free
//     for (i = 0; i < rq; i++)
//       if (mm_rqs[i].address)
//         sys_free(mm_rqs[i].address);

//     // Mostrar estado de memoria cada iteración
//     MemStatus st = sys_memstatus();
//     printf("[MM] total=%u used=%u free=%u blocks=%u\n", (unsigned)st.totalMemory, (unsigned)st.usedMemory, (unsigned)st.freeMemory, (unsigned)st.allocatedBlocks);
//   }
// }




#include "../include/test_mm.h"

#define MAX_BLOCKS 128
#define MAX_ITERATIONS 1000
#define REPORT_INTERVAL 100  // Reportar cada 100 iteraciones

typedef struct MM_rq {
  void *address;
  uint32_t size;
} mm_rq;

static uint32_t m_z = 362436069;
static uint32_t m_w = 521288629;

uint32_t GetUint() {
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  return (m_z << 16) + m_w;
}

uint32_t GetUniform(uint32_t max) {
  uint32_t u = GetUint();
  return (u + 1.0) * 2.328306435454494e-10 * max;
}

int64_t satoi(char *str) {
  uint64_t i = 0;
  int64_t res = 0;
  int8_t sign = 1;

  if (!str)
    return 0;

  if (str[i] == '-') {
    i++;
    sign = -1;
  }

  for (; str[i] != '\0'; ++i) {
    if (str[i] < '0' || str[i] > '9')
      return 0;
    res = res * 10 + str[i] - '0';
  }

  return res * sign;
}

uint8_t memcheck(void *start, uint8_t value, uint32_t size) {
  uint8_t *p = (uint8_t *)start;
  uint32_t i;

  for (i = 0; i < size; i++, p++)
    if (*p != value)
      return 0;

  return 1;
}

// Nueva función: Verificar que los bloques no se solapan
uint8_t check_overlap(mm_rq *rqs, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (!rqs[i].address) continue;
    
    char *start_i = (char *)rqs[i].address;
    char *end_i = start_i + rqs[i].size;
    
    for (uint8_t j = i + 1; j < count; j++) {
      if (!rqs[j].address) continue;
      
      char *start_j = (char *)rqs[j].address;
      char *end_j = start_j + rqs[j].size;
      
      // Verificar solapamiento
      if ((start_i < end_j) && (start_j < end_i)) {
        printf("ERROR: Blocks %d and %d overlap!\n", i, j);
        printf("  Block %d: 0x%x - 0x%x (%u bytes)\n", i, start_i, end_i, rqs[i].size);
        printf("  Block %d: 0x%x - 0x%x (%u bytes)\n", j, start_j, end_j, rqs[j].size);
        return 0;
      }
    }
  }
  return 1;
}

int64_t test_mm(uint64_t argc, char *argv[]) {
  mm_rq mm_rqs[MAX_BLOCKS];
  uint8_t rq;
  uint32_t total;
  uint64_t max_memory;
  int iteration = 0;
  
  // Estadísticas
  uint32_t total_allocations = 0;
  uint32_t failed_allocations = 0;
  uint32_t max_blocks_allocated = 0;
  uint32_t max_memory_used = 0;

  if (argc != 1) {
    printf("Usage: test mm <max_memory>\n");
    return -1;
  }

  if ((max_memory = satoi(argv[0])) <= 0) {
    printf("ERROR: Invalid max_memory value\n");
    return -1;
  }

  printf("\n");
  printf("========================================\n");
  printf("   MEMORY MANAGER STRESS TEST\n");
  printf("========================================\n");
  printf("Max memory per iteration: %u bytes\n", (unsigned)max_memory);
  printf("Max blocks per iteration: %d\n", MAX_BLOCKS);
  printf("Total iterations: %d\n", MAX_ITERATIONS);
  printf("========================================\n\n");

  // Estado inicial
  MemStatus st_initial = sys_memstatus();
  printf("Initial state:\n");
  printf("  Total: %u bytes (%.2f MB)\n", 
         (unsigned)st_initial.totalMemory, 
         (float)st_initial.totalMemory / (1024.0 * 1024.0));
  printf("  Free:  %u bytes (%.2f MB)\n", 
         (unsigned)st_initial.freeMemory,
         (float)st_initial.freeMemory / (1024.0 * 1024.0));
  printf("\n");

  // Loop principal
  while (iteration < MAX_ITERATIONS) {
    rq = 0;
    total = 0;

    // ===== FASE 1: ALLOCACIÓN =====
    while (rq < MAX_BLOCKS && total < max_memory) {
      mm_rqs[rq].size = GetUniform(max_memory - total - 1) + 1;
      mm_rqs[rq].address = sys_malloc(mm_rqs[rq].size);

      total_allocations++;

      if (mm_rqs[rq].address) {
        total += mm_rqs[rq].size;
        rq++;
      } else {
        failed_allocations++;
        // Si falla muy temprano, es un error
        if (total < max_memory / 2) {
          printf("\nERROR: malloc failed too early!\n");
          printf("  Iteration: %d\n", iteration);
          printf("  Requested: %u bytes\n", mm_rqs[rq].size);
          printf("  Already allocated: %u bytes\n", total);
          printf("  Blocks allocated: %u\n", rq);
          return -1;
        }
        break;
      }
    }

    // Estadísticas
    if (rq > max_blocks_allocated)
      max_blocks_allocated = rq;
    if (total > max_memory_used)
      max_memory_used = total;

    // ===== FASE 2: VERIFICAR NO SOLAPAMIENTO =====
    if (!check_overlap(mm_rqs, rq)) {
      printf("FATAL: Memory overlap detected at iteration %d\n", iteration);
      return -1;
    }

    // ===== FASE 3: ESCRIBIR DATOS =====
    uint32_t i;
    for (i = 0; i < rq; i++) {
      if (mm_rqs[i].address) {
        memset(mm_rqs[i].address, i & 0xFF, mm_rqs[i].size);
      }
    }

    // ===== FASE 4: VERIFICAR INTEGRIDAD =====
    for (i = 0; i < rq; i++) {
      if (mm_rqs[i].address) {
        if (!memcheck(mm_rqs[i].address, i & 0xFF, mm_rqs[i].size)) {
          printf("\nERROR: Memory corruption detected!\n");
          printf("  Iteration: %d\n", iteration);
          printf("  Block: %u\n", i);
          printf("  Address: 0x%x\n", mm_rqs[i].address);
          printf("  Size: %u bytes\n", mm_rqs[i].size);
          printf("  Expected value: 0x%02x\n", i & 0xFF);
          return -1;
        }
      }
    }

    // ===== FASE 5: LIBERAR (patrón alternado para fragmentación) =====
    // Primero liberar los pares
    for (i = 0; i < rq; i += 2) {
      if (mm_rqs[i].address) {
        sys_free(mm_rqs[i].address);
        mm_rqs[i].address = NULL;
      }
    }
    
    // Luego liberar los impares
    for (i = 1; i < rq; i += 2) {
      if (mm_rqs[i].address) {
        sys_free(mm_rqs[i].address);
        mm_rqs[i].address = NULL;
      }
    }

    // ===== FASE 6: VERIFICAR ESTADO FINAL =====
    MemStatus st = sys_memstatus();
    
    // CRÍTICO: Verificar que toda la memoria fue liberada
    if (st.usedMemory != 0) {
      printf("\nERROR: MEMORY LEAK DETECTED!\n");
      printf("  Iteration: %d\n", iteration);
      printf("  Used memory: %u bytes (should be 0)\n", (unsigned)st.usedMemory);
      printf("  Allocated blocks: %u (should be 0)\n", (unsigned)st.allocatedBlocks);
      return -1;
    }

    if (st.allocatedBlocks != 0) {
      printf("\nERROR: BLOCK LEAK DETECTED!\n");
      printf("  Iteration: %d\n", iteration);
      printf("  Allocated blocks: %u (should be 0)\n", (unsigned)st.allocatedBlocks);
      return -1;
    }

    // Verificar que la memoria libre es la misma que al inicio
    if (st.freeMemory < st_initial.freeMemory - 1000) { // Margen de error pequeño
      printf("\nWARNING: Free memory decreased!\n");
      printf("  Initial: %u bytes\n", (unsigned)st_initial.freeMemory);
      printf("  Current: %u bytes\n", (unsigned)st.freeMemory);
      printf("  Difference: %d bytes\n", (int)(st_initial.freeMemory - st.freeMemory));
    }

    // Reporte periódico
    if (iteration % REPORT_INTERVAL == 0) {
      printf("[Iteration %4d] ", iteration);
      printf("Allocated: %3u blocks, %6u bytes | ", rq, total);
      printf("Status: ");
      if (st.usedMemory == 0 && st.allocatedBlocks == 0) {
        printf("OK ✓\n");
      } else {
        printf("LEAK! used=%u blocks=%u\n", 
               (unsigned)st.usedMemory, 
               (unsigned)st.allocatedBlocks);
      }
    }

    iteration++;
  }

  // ===== REPORTE FINAL =====
  printf("\n");
  printf("========================================\n");
  printf("         TEST COMPLETED\n");
  printf("========================================\n");
  printf("Result: ALL TESTS PASSED ✓\n");
  printf("\n");
  printf("Statistics:\n");
  printf("  Total iterations:     %d\n", iteration);
  printf("  Total allocations:    %u\n", total_allocations);
  printf("  Failed allocations:   %u (%.2f%%)\n", 
         failed_allocations,
         (float)failed_allocations / total_allocations * 100);
  printf("  Max blocks used:      %u / %d\n", max_blocks_allocated, MAX_BLOCKS);
  printf("  Max memory used:      %u bytes\n", max_memory_used);
  printf("\n");
  printf("Memory integrity:       ✓ VERIFIED\n");
  printf("No overlapping blocks:  ✓ VERIFIED\n");
  printf("No memory leaks:        ✓ VERIFIED\n");
  printf("No block leaks:         ✓ VERIFIED\n");
  printf("\n");
  
  MemStatus st_final = sys_memstatus();
  printf("Final state:\n");
  printf("  Used:    %u bytes\n", (unsigned)st_final.usedMemory);
  printf("  Free:    %u bytes\n", (unsigned)st_final.freeMemory);
  printf("  Blocks:  %u\n", (unsigned)st_final.allocatedBlocks);
  
  if (st_final.usedMemory == 0 && st_final.allocatedBlocks == 0) {
    printf("\n✓ Memory manager is working correctly!\n");
  }
  
  printf("========================================\n");
  
  return 0;
}