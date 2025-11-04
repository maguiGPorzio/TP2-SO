#include "usrlib.h"

uint64_t num_to_str(uint64_t value, char * buffer, uint32_t base) {
	char *p = buffer;
	char *p1, *p2;
	uint64_t digits = 0;

	//Calculate characters for each digit
	do{
		uint32_t remainder = value % base;
		*p++ = (remainder < 10) ? remainder + '0' : remainder + 'A' - 10;
		digits++;
	}
	while (value /= base);

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

uint64_t strlen(char * str) {
    uint64_t len = 0;
    while (str[len] != 0) {
        len++;
    }

    return len;
}


int strcmp(char * s1, char * s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (uint8_t)(*s1) - (uint8_t)(*s2);
}

// Parameters
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