#ifndef _SYSCALL_DISPATCHER_H_
#define _SYSCALL_DISPATCHER_H_

#include <stdint.h>
#include <stddef.h>
#include "memoryManager.h"

#define STDOUT 1
#define STDERR 2

#define SYSCALL_COUNT 35 // actualizar también el chequeo en asm/interrupts.asm (31 reservado no-op)

extern void * syscalls[SYSCALL_COUNT];

// pueden recibir hasta 3 argumentos
static uint64_t sys_write(uint64_t fd, const char * buf, uint64_t count);
static uint64_t sys_read(char * buf, uint64_t count);
static void sys_date(uint8_t * buffer);
static void sys_time(uint8_t * buffer);
static uint64_t sys_regs(char * buffer);
static void sys_clear();
static void sys_increase_fontsize();
static void sys_decrease_fontsize() ;
static void sys_beep(uint32_t freq_hz, uint64_t duration);
static void sys_screensize(uint32_t * width, uint32_t * height);
static void sys_circle(uint64_t fill, uint64_t * info, uint32_t color);
static void sys_rectangle(uint64_t fill, uint64_t * info, uint32_t color);
static void sys_draw_line(uint64_t * info, uint32_t color);
static void sys_draw_string(const char * buf, uint64_t * info, uint32_t color);
static void sys_speaker_start(uint32_t freq_hz);
static void sys_speaker_stop();
static void sys_textmode();
static void sys_videomode();
static void sys_putpixel(uint32_t hexColor, uint64_t x, uint64_t y);
static uint64_t sys_key_status(char c);
static void sys_sleep(uint64_t miliseconds);
static void sys_clear_input_buffer();
static uint64_t sys_ticks();

// Memory management syscalls
static void * sys_malloc(size_t size);
static void sys_free(void * ptr);
static MemStatus sys_memStatus(void);

// processes syscalls (scheduler-based)
static int64_t sys_create_process(void * entry, int argc, const char **argv, const char *name);
static void    sys_exit_current(int status);
static int64_t sys_getpid(void);
static int64_t sys_kill(int pid);
static int64_t sys_block(int pid);
static int64_t sys_unblock(int pid);
static int64_t sys_wait(int pid);
static int64_t sys_nice(int64_t pid, int new_prio);

#endif