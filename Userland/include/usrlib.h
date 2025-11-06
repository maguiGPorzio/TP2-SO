#ifndef _USR_LIB_H_
#define _USR_LIB_H_

#include <stdint.h>
#include "syscalls.h"

#define MIN_PRIORITY 1
#define MAX_PRIORITY 5 // TODO: ver donde poner esto porque lo comparte con Kernel también


#define REGSBUF_SIZE 800

// Cantidad de registros para argumentos de syscalls
#define NUM_INT_REGS 5 // 5 registros para enteros (rbx, rcx, rdx, rsi, rdi)
#define NUM_SSE_REGS 8 // SSE = Floats y Doubles

#define FLOAT_PRECISION 6 

#define BINARY_BUFFER_SIZE  65  // 64 bits + terminador nulo
#define OCTAL_BUFFER_SIZE   23  // 22 digitos + terminador nulo
#define DECIMAL_BUFFER_SIZE 21  // 20 digitos + terminador nulo
#define HEX_BUFFER_SIZE     17  // 16 digitos + terminador nulo


extern void generate_invalid_opcode();

/*-- FUNCIONES DE I/O --*/
uint64_t print(char *str);
uint64_t print_err(char *str);
uint64_t putchar(char c);
char getchar(void);
uint64_t printf_aux(const char *fmt, const uint64_t *regArgs, const uint64_t *stackPtr, const double *floatArgs);
uint64_t get_key_status(char key);
uint64_t fprint(uint64_t fd, char *str);
extern uint64_t printf(const char *fmt, ...);
extern uint64_t scanf(const char *fmt, ...);

/*-- FUNCIONES PARA DIBUJAR --*/
void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);
void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);
void draw_string(char * str, uint64_t x, uint64_t y, uint64_t size, uint32_t color);
void draw_line(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);


/*-- FUNCIONES DE STRINGS --*/
uint64_t strlen(char * str);
int strcmp(char * s1, char * s2);
uint64_t num_to_str(uint64_t value, char * buffer, uint32_t base);
int64_t satoi(char *str);

/*-- FUNCIONES DE MATEMATICAS --*/
float inv_sqrt(float number);
uint32_t GetUint();
uint32_t GetUniform(uint32_t max);

#endif 