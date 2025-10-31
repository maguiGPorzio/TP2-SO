#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    size_t allocated_blocks;
} MemStatus;

typedef int (*process_entry_t)(int argc, char **argv);

/*-- SYSTEMCALLS DE ARQUI --*/
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
extern int64_t sys_create_process(void * entry, int argc, const char **argv, const char *name, int fds[2]);
extern void sys_exit(int status);
extern int64_t sys_getpid(void);
extern int64_t sys_kill(int64_t pid);
extern int64_t sys_block(int64_t pid);
extern int64_t sys_unblock(int64_t pid);
extern int64_t sys_wait(int64_t pid);
extern int64_t sys_nice(int64_t pid, int new_prio);
extern void sys_yield();
extern void sys_print_processes();

/*-- SYSTEMCALLS DE SEMAFOROS --*/
extern int64_t sys_sem_open(const char *name, int value);
extern void sys_sem_close(int sem_id);
extern void sys_sem_wait(int sem_id);
extern void sys_sem_post(int sem_id);

/*-- SYSTEMCALLS DE PIPES --*/
extern int sys_create_pipe(int fds[2]);
extern void sys_destroy_pipe(int fd);
