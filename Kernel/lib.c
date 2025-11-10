// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdint.h>

void * memset(void * destination, int32_t c, uint64_t length){
	uint8_t chr = (uint8_t)c;
	char * dst = (char*)destination;

	while(length--)
		dst[length] = chr;

	return destination;
}

void * memcpy(void * destination, const void * source, uint64_t length){
	/*
	* memcpy does not support overlapping buffers, so always do it
	* forwards. (Don't change this without adjusting memmove.)
	*
	* For speedy copying, optimize the common case where both pointers
	* and the length are word-aligned, and copy word-at-a-time instead
	* of byte-at-a-time. Otherwise, copy by bytes.
	*
	* The alignment logic below should be portable. We rely on
	* the compiler to be reasonably intelligent about optimizing
	* the divides and modulos out. Fortunately, it is.
	*/
	uint64_t i = 0;
	uint8_t * d = (uint8_t*)destination;
	const uint8_t * s = (const uint8_t*)source;

	// Copy byte by byte until aligned to 8 bytes
	while (i < length && ((uint64_t)(d + i) % sizeof(uint64_t) != 0 || (uint64_t)(s + i) % sizeof(uint64_t) != 0)){
		d[i] = s[i];
		i++;
	}

	// Copy 8 bytes at a time when aligned
	uint64_t *d64 = (uint64_t *)(d + i);
	const uint64_t *s64 = (const uint64_t *)(s + i);
	uint64_t remaining = length - i;
	uint64_t aligned_count = remaining / sizeof(uint64_t);

	for (uint64_t j = 0; j < aligned_count; j++){
		d64[j] = s64[j];
	}

	i += aligned_count * sizeof(uint64_t);

	// Copy remaining bytes
	while (i < length){
		d[i] = s[i];
		i++;
	}

	return destination;
}

void * memset64(void * destination, uint64_t pattern, uint64_t length){
	uint8_t *d = (uint8_t *)destination;
	uint64_t i = 0;

	// Write bytes until destination is 8-byte aligned or until no bytes left
	while (i < length && ((uint64_t)(d + i) % sizeof(uint64_t) != 0)) {
		d[i++] = (uint8_t)pattern; // Use LSB of pattern for tail bytes
	}

	// Now destination is aligned or no more bytes left
	uint64_t remaining = length - i;
	uint64_t words = remaining / sizeof(uint64_t);

	uint64_t *d64 = (uint64_t *)(d + i);
	for (uint64_t j = 0; j < words; j++) {
		d64[j] = pattern;
	}

	i += words * sizeof(uint64_t);

	// Trailing bytes
	while (i < length) {
		d[i++] = (uint8_t)pattern;
	}

	return destination;
}

// Copia hasta n caracteres de src a dst
char *strncpy(char *dst, const char *src, int n) {
    char *ret = dst;
    while (n > 0 && *src) {
        *dst++ = *src++;
        n--;
    }
    while (n > 0) {
        *dst++ = '\0'; // Rellenamos con '\0' si src terminó antes
        n--;
    }
    return ret;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *) s1 - *(const unsigned char *) s2;
}

int strlen(const char *str) {
	int res = 0;
	while(*str++) {
		res++;
	}
	return res;
}

uint64_t decimal_to_str(uint64_t value, char * buffer) {
	char *p = buffer;
	char *p1, *p2;
	uint64_t digits = 0;

	//Calculate characters for each digit
	do{
		uint32_t remainder = value % 10;
		*p++ = remainder + '0';
		digits++;
	}
	while (value /= 10);

	*p = 0;

	p1 = buffer;
	p2 = p - 1;
	while (p1 < p2){
		char tmp = *p1;
		*p1 = *p2;
		*p2 = tmp;
		p1++;
		p2--;
	}
	return digits;
}

int strcat(char *dest, const char *src) {
	int i = strlen(dest);
	while (*src) {
		dest[i++] = *src++;
	}
	dest[i] = '\0';
	return i;
}