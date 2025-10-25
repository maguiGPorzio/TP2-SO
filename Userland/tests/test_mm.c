//TEST CATEDRA
#include "../include/test_mm.h" // para sys_* y MemStatus

#define MAX_BLOCKS 128

typedef struct MM_rq {
  void *address;
  uint32_t size;
} mm_rq;

static uint32_t m_z = 362436069;
static uint32_t m_w = 521288629;

static uint32_t GetUint() {
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  return (m_z << 16) + m_w;
}

static uint32_t GetUniform(uint32_t max) {
  uint32_t u = GetUint();
  return (u + 1.0) * 2.328306435454494e-10 * max;
}

static int64_t satoi(char *str) {
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


int64_t test_mm(uint64_t argc, char * argv[]){

  mm_rq mm_rqs[MAX_BLOCKS];
  uint8_t rq;
  uint32_t total;
  uint64_t max_memory;

  if (argc != 1)
    return -1;

  if ((max_memory = satoi(argv[0])) <= 0)
    return -1;

  while (1) {
    rq = 0;
    total = 0;

    // Request as many blocks as we can
    while (rq < MAX_BLOCKS && total < max_memory) {
      mm_rqs[rq].size = GetUniform(max_memory - total - 1) + 1;
  mm_rqs[rq].address = sys_malloc(mm_rqs[rq].size);

      if (mm_rqs[rq].address) {
        total += mm_rqs[rq].size;
        rq++;
      }
    }

    // Set
    uint32_t i;
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        memset(mm_rqs[i].address, i, mm_rqs[i].size);

    // Check
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        if (!memcheck(mm_rqs[i].address, i, mm_rqs[i].size)) {
          printf("test_mm ERROR\n");
          return -1;
        }

    // Free
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        sys_free(mm_rqs[i].address);

    // Mostrar estado de memoria cada iteración
    MemStatus st = sys_memstatus();
    printf("[MM] total=%u used=%u free=%u blocks=%u\n", (unsigned)st.totalMemory, (unsigned)st.usedMemory, (unsigned)st.freeMemory, (unsigned)st.allocatedBlocks);
  }
}


//TEST CATEDRA MEJORADO
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

// int64_t test_mm(uint64_t argc, char *argv[]) {
//   mm_rq mm_rqs[MAX_BLOCKS];
//   uint8_t rq;
//   uint32_t total;
//   uint64_t max_memory;
//   uint32_t iteration = 0;

//   if (argc != 1)
//     return -1;

//   if ((max_memory = satoi(argv[0])) <= 0)
//     return -1;

//   // Estado inicial
//   MemStatus initial_status = sys_memstatus();
//   printf("\n========================================\n");
//   printf("   MEMORY MANAGER STRESS TEST\n");
//   printf("========================================\n");
//   printf("Max memory per iteration: %u bytes\n", (unsigned)max_memory);
//   printf("Max blocks per iteration: %d\n", MAX_BLOCKS);
//   printf("\nInitial state:\n");
//   printf("  Total: %u bytes\n", (unsigned)initial_status.totalMemory);
//   printf("  Free:  %u bytes\n", (unsigned)initial_status.freeMemory);
//   printf("  Used:  %u bytes\n", (unsigned)initial_status.usedMemory);
//   printf("  Blocks: %u\n", (unsigned)initial_status.allocatedBlocks);
//   printf("========================================\n\n");

//   while (1) {
//     rq = 0;
//     total = 0;

//     // ===== FASE 1: ALLOCACIÓN =====
//     while (rq < MAX_BLOCKS && total < max_memory) {
//       mm_rqs[rq].size = GetUniform(max_memory - total - 1) + 1;
//       mm_rqs[rq].address = sys_malloc(mm_rqs[rq].size);

//       if (mm_rqs[rq].address) {
//         total += mm_rqs[rq].size;
//         rq++;
//       }
//     }

//     // ===== FASE 2: ESCRIBIR DATOS =====
//     uint32_t i;
//     for (i = 0; i < rq; i++) {
//       if (mm_rqs[i].address) {
//         memset(mm_rqs[i].address, i & 0xFF, mm_rqs[i].size);
//       }
//     }

//     // ===== FASE 3: VERIFICAR INTEGRIDAD =====
//     for (i = 0; i < rq; i++) {
//       if (mm_rqs[i].address) {
//         if (!memcheck(mm_rqs[i].address, i & 0xFF, mm_rqs[i].size)) {
//           printf("\n ERROR: Memory corruption detected!\n");
//           printf("  Iteration: %u\n", iteration);
//           printf("  Block: %u\n", i);
//           printf("  Address: %p\n", mm_rqs[i].address);
//           printf("  Size: %u bytes\n", mm_rqs[i].size);
//           return -1;
//         }
//       }
//     }

//     // ===== FASE 4: LIBERAR TODO =====
//     for (i = 0; i < rq; i++) {
//       if (mm_rqs[i].address) {
//         sys_free(mm_rqs[i].address);
//         mm_rqs[i].address = NULL;  // Limpiar puntero
//       }
//     }

//     // ===== FASE 5: VERIFICAR ESTADO DESPUÉS DE FREE =====
//     MemStatus st = sys_memstatus();

//     // CRÍTICO: Verificar que toda la memoria fue liberada
//     if (st.usedMemory != initial_status.usedMemory) {
//       printf("\n❌ ERROR: MEMORY LEAK DETECTED!\n");
//       printf("  Iteration: %u\n", iteration);
//       printf("  Initial used: %u bytes\n", (unsigned)initial_status.usedMemory);
//       printf("  Current used: %u bytes\n", (unsigned)st.usedMemory);
//       printf("  Leaked: %u bytes\n", 
//              (unsigned)(st.usedMemory - initial_status.usedMemory));
//       printf("  Allocated blocks: %u (should be %u)\n", 
//              (unsigned)st.allocatedBlocks,
//              (unsigned)initial_status.allocatedBlocks);
//       return -1;
//     }

//     // Reporte periódico cada 100 iteraciones
//     if (iteration % 100 == 0) {
//       printf("[Iteration %5u] ", iteration);
//       printf("Allocated: %3u blocks, %6u bytes | ", rq, total);
//       printf("Status: ");
      
//       if (st.usedMemory == initial_status.usedMemory && 
//           st.allocatedBlocks == initial_status.allocatedBlocks) {
//         printf("✓ OK (no leaks)\n");
//       } else {
//         printf("⚠ LEAK! used=%u blocks=%u\n", 
//                (unsigned)st.usedMemory, 
//                (unsigned)st.allocatedBlocks);
//       }
//     }

//     iteration++;
//   }

//   return 0;
// }