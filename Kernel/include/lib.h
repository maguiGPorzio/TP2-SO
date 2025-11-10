#ifndef LIB_H
#define LIB_H

#include <stdint.h>

void *memset(void *destination, int32_t character, uint64_t length);
void *memcpy(void *destination, const void *source, uint64_t length);

// Fills memory with a repeating 64-bit pattern. Length is in bytes.
// For any trailing bytes (length % 8), the low 8 bits of the pattern are used per byte.
void *memset64(void *destination, uint64_t pattern, uint64_t length);

char *strncpy(char *dst, const char *src, int n);
char  strlen(const char *str);
int   strcmp(const char *s1, const char *s2);

uint64_t decimal_to_str(uint64_t value, char *buffer);
int      strcat(char *dest, const char *src);

#endif