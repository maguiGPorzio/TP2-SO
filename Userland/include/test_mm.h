
#ifndef TEST_MM_H
#define TEST_MM_H

#include "usrlib.h" // para sys_* y MemStatus

#define MAX_BLOCKS 20

void * memset(void * destination, int32_t character, uint64_t length);
void * memcpy(void * destination, const void * source, uint64_t length);

int64_t test_mm(uint64_t argc, char * argv[]);


#endif