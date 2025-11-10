#ifndef _SYSCALL_DISPATCHER_H_
#define _SYSCALL_DISPATCHER_H_

#include <stdint.h>
#include <stddef.h>
#include "memory_manager.h"
#include "process.h"
#include "pipes.h"

// syscalls de arqui
static int      sys_write(uint64_t fd, const char *buf, uint64_t count);
static int      sys_read(int fd, char *buf, uint64_t count);
static void     sys_date(uint8_t *buffer);
static void     sys_time(uint8_t *buffer);
static uint64_t sys_regs(char *buffer);
static void     sys_clear();
static void     sys_increase_fontsize();
static void     sys_decrease_fontsize();
static void     sys_beep(uint32_t freq_hz, uint64_t duration);
static void     sys_screensize(uint32_t *width, uint32_t *height);
static void     sys_circle(uint64_t fill, uint64_t *info, uint32_t color);
static void     sys_rectangle(uint64_t fill, uint64_t *info, uint32_t color);
static void     sys_draw_line(uint64_t *info, uint32_t color);
static void     sys_draw_string(const char *buf, uint64_t *info, uint32_t color);
static void     sys_speaker_start(uint32_t freq_hz);
static void     sys_speaker_stop();
static void     sys_textmode();
static void     sys_videomode();
static void     sys_put_pixel(uint32_t hex_color, uint64_t x, uint64_t y);
static uint64_t sys_key_status(char c);
static void     sys_sleep(uint64_t miliseconds);
static void     sys_clear_input_buffer();
static uint64_t sys_ticks();

// syscalls de memory management
static void      *sys_malloc(size_t size);
static void       sys_free(void *ptr);
static mem_info_t sys_mem_info(void);

// syscalls de procesos
static int64_t
sys_create_process(void *entry, int argc, const char **argv, const char *name, int fds[2]);
static void    sys_exit(int status);
static int64_t sys_getpid(void);
static int64_t sys_kill(int pid);
static int64_t sys_block(int pid);
static int64_t sys_unblock(int pid);
static int64_t sys_wait(int pid);
static int64_t sys_nice(int pid, int new_prio);
static void    sys_yield();
static int     sys_processes_info(process_info_t *buf, int max_count);

// syscalls para foreground processes
static int sys_set_foreground_process(int pid);
static int sys_adopt_init_as_parent(int pid);
static int sys_get_foreground_process(void);

// syscalls de semaforos
static int64_t sys_sem_open(const char *name, int value);
static void    sys_sem_close(const char *name);
static void    sys_sem_wait(const char *name);
static void    sys_sem_post(const char *name);

// syscalls de pipes
static int  sys_create_pipe(int fds[2]);
static void sys_destroy_pipe(int id);
static int  sys_open_named_pipe(char *name, int fds[2]);
static int  sys_close_fd(int fd);
static int  sys_pipes_info(pipe_info_t *buf, int max_count);

#endif