#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

#define OK 0
#define ERROR -1
#define EOF -1
#define MAX_NAME_LENGTH 32
#define MAX_PROCESSES 64

#define MIN_PRIORITY 2
#define MAX_PRIORITY 0

#define MAX_PIPES 32
#define MAX_PIPE_NAME_LENGTH 32

enum { STDIN = 0, STDOUT, STDERR, STDGREEN, STDBLUE, STDCYAN, STDMAGENTA, STDYELLOW, FDS_COUNT };

typedef struct mem_info {
	size_t total_memory;
	size_t used_memory;
	size_t free_memory;
	size_t allocated_blocks;
} mem_info_t;

typedef int (*process_entry_t)(int argc, char **argv);

typedef enum { PS_READY = 0, PS_RUNNING, PS_BLOCKED, PS_TERMINATED } process_status_t;

typedef struct process_info {
	int              pid;
	char             name[MAX_NAME_LENGTH];
	process_status_t status;
	uint8_t          priority;
	int              parent_pid;
	int              read_fd;
	int              write_fd;
	uint64_t         stack_base;
	uint64_t         stack_pointer;
} process_info_t;

typedef struct pipe_info {
	int  id;
	char name[MAX_PIPE_NAME_LENGTH];
	int  read_fd;
	int  write_fd;
	int  readers;
	int  writers;
	int  buffered;
} pipe_info_t;

// syscalls de arqui
extern uint64_t sys_regs(char *buf);
extern void     sys_time(uint8_t *buf);
extern void     sys_date(uint8_t *buf);
extern int      sys_read(int fd, char *buf, uint64_t count);
extern int      sys_write(uint64_t fd, const char *buf, uint64_t count);
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
extern void     sys_enable_textmode();
extern void     sys_disable_textmode();
extern void     sys_put_pixel(uint32_t color, uint64_t x, uint64_t y);
extern uint64_t sys_key_status(char key);
extern void     sys_sleep(uint64_t miliseconds);
extern void     sys_clear_input_buffer();
extern uint64_t sys_ticks();

// syscalls de memory management
extern void      *sys_malloc(uint64_t size);
extern void       sys_free(void *ptr);
extern mem_info_t sys_mem_info(void);

// syscalls de procesos
extern int64_t
sys_create_process(void *entry, int argc, const char **argv, const char *name, int fds[2]);
extern void    sys_exit(int status);
extern int64_t sys_getpid(void);
extern int64_t sys_kill(int pid);
extern int64_t sys_block(int pid);
extern int64_t sys_unblock(int pid);
extern int64_t sys_wait(int pid);
extern int64_t sys_nice(int pid, int new_prio);
extern void    sys_yield();
extern int     sys_processes_info(process_info_t *buf, int max_count);

// syscalls para foreground process
extern int sys_set_foreground_process(int pid);
extern int sys_adopt_init_as_parent(int pid);
extern int sys_get_foreground_process(void);

// syscalls de semaforos
extern int64_t sys_sem_open(const char *name, int value);
extern void    sys_sem_close(const char *name);
extern void    sys_sem_wait(const char *name);
extern void    sys_sem_post(const char *name);

// syscalls de pipes
extern int  sys_create_pipe(int fds[2]);
extern void sys_destroy_pipe(int fd);
extern int  sys_open_named_pipe(char *name, int fds[2]);
extern int  sys_close_fd(int fd);
extern int  sys_pipes_info(pipe_info_t *buf, int max_count);

#endif