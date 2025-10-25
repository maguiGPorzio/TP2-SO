#ifndef _USR_LIB_H_
#define _USR_LIB_H_

#include <stdint.h>
#include "commonTypes.h"

// File descriptors
#define STDOUT  1
#define STDERR  2

#define REGSBUF_SIZE 800

// Cantidad de registros para argumentos de syscalls
#define NUM_INT_REGS 5 // 5 registros para enteros (rbx, rcx, rdx, rsi, rdi)
#define NUM_SSE_REGS 8 // SSE = Floats y Doubles

#define FLOAT_PRECISION 6 

#define BINARY_BUFFER_SIZE  65  // 64 bits + terminador nulo
#define OCTAL_BUFFER_SIZE   23  // 22 digitos + terminador nulo
#define DECIMAL_BUFFER_SIZE 21  // 20 digitos + terminador nulo
#define HEX_BUFFER_SIZE     17  // 16 digitos + terminador nulo

/*-- SYSTEMCALLS DIRECTO DE ASM --*/
extern uint64_t sys_regs(char *buf);
extern void     sys_time(uint8_t *buf);     
extern void     sys_date(uint8_t *buf);
extern uint64_t sys_read(char *buf,  uint64_t count);
extern uint64_t sys_write(uint64_t fd, const char *buf, uint64_t count);
extern void     sys_increase_fontsize();
extern void     sys_decrease_fontsize();
extern void     sys_beep(uint32_t freq_hz, uint64_t duration_ms);
extern void     sys_screen_size(uint32_t *width, uint32_t *height);
extern void     sys_circle(uint64_t fill, uint64_t *info, uint32_t color);
extern void     sys_rectangle(uint64_t fill, uint64_t *info, uint32_t color);
extern void     sys_line(uint64_t *info, uint32_t color);
extern void     sys_draw_string(const char *s, uint64_t *info, uint32_t color);
extern void     sys_clear(void);
extern void     sys_speaker_start(uint32_t freq_hz);
extern void     sys_speaker_stop(void);
extern void 	sys_enable_textmode();
extern void 	sys_disable_textmode();
extern void     sys_put_pixel(uint32_t color, uint64_t x, uint64_t y);
extern uint64_t sys_key_status(char key);
extern void     sys_sleep(uint64_t miliseconds);
extern void     sys_clear_input_buffer();
extern uint64_t sys_ticks();
extern void generate_invalid_opcode();
extern uint64_t printf(const char *fmt, ...);
extern uint64_t scanf(const char *fmt, ...);

/*-- SYSTEMCALLS DE MEMORIA --*/
extern void * sys_malloc(uint64_t size);
extern void sys_free(void * ptr);
extern MemStatus sys_memstatus(void);

/*-- SYSTEMCALLS DE PROCESOS --*/
// Nuevos syscalls de procesos (kernel indices 26..32)
extern int64_t sys_create_process(void * entry, int argc, const char **argv, const char *name);
extern void    sys_exit_current(int status);
extern int64_t sys_getpid(void);
extern int64_t sys_kill(int pid);
extern int64_t sys_block(int pid);
extern int64_t sys_unblock(int pid);

/* Wrappers esperados por tests de cátedra (mapean a sys_* o stubs) */
int64_t my_create_process(const char *name, uint64_t argc, char *argv[]);
int64_t my_wait(int64_t pid);
int64_t my_nice(int64_t pid, int new_prio);
int64_t my_block(int64_t pid);
int64_t my_unblock(int64_t pid);
int64_t my_kill(int64_t pid);
int64_t my_getpid(void);

/*-- FUNCIONES DE I/O --*/
uint64_t print_err(char *str);
uint64_t print_string(char *str);
uint64_t putchar(char c);
char getchar(void);
char getchar_nonblock(); // no se queda esperando una tecla, si no hay devuelve 0
uint64_t printf_aux(const char *fmt, const uint64_t *regArgs, const uint64_t *stackPtr, const double *floatArgs);
uint64_t get_key_status(char key);

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

/*-- FUNCIONES DE MATEMATICAS --*/
float inv_sqrt(float number);

#endif 