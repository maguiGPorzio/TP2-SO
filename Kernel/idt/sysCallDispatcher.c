// Clean implementation with process syscalls added
#include <stddef.h>
#include <stdint.h>
#include "videoDriver.h"
#include "time.h"
#include "keyboard.h"
#include "sound.h"
#include "sysCallDispatcher.h"
#include "memoryManager.h"
#include "process.h"
#include "scheduler.h"
#include "synchro.h"
#include "pipes.h"

#define MIN_CHAR 0
#define MAX_CHAR 256

void * syscalls[] = {
    // syscalls de arqui
    &sys_regs,               // 0
    &sys_time,               // 1
    &sys_date,               // 2
    &sys_read,               // 3
    &sys_write,              // 4
    &sys_increase_fontsize,  // 5
    &sys_decrease_fontsize,  // 6
    &sys_beep,               // 7
    &sys_screensize,         // 8
    &sys_circle,             // 9
    &sys_rectangle,          // 10
    &sys_draw_line,          // 11
    &sys_draw_string,        // 12
    &sys_clear,              // 13
    &sys_speaker_start,      // 14
    &sys_speaker_stop,       // 15
    &sys_textmode,           // 16
    &sys_videomode,          // 17
    &sys_putpixel,           // 18
    &sys_key_status,         // 19
    &sys_sleep,              // 20
    &sys_clear_input_buffer, // 21
    &sys_ticks,              // 22

    // syscalls de memoria
    &sys_malloc,             // 23
    &sys_free,               // 24
    &sys_memStatus,          // 25

    // syscalls de procesos
    &sys_create_process,     // 26
    &sys_exit,               // 27
    &sys_getpid,             // 28
    &sys_kill,               // 29
    &sys_block,              // 30
    &sys_unblock,            // 31
    &sys_wait,               // 32
    &sys_nice,               // 33
    &sys_yield,              // 34
    &sys_print_processes,    // 35

    // syscalls de semaforos 
    &sys_sem_open,           // 36
    &sys_sem_close,          // 37
    &sys_sem_wait,           // 38
    &sys_sem_post,           // 39

    // syscalls de pipes
    &sys_create_pipe,        // 40
    &sys_destroy_pipe        // 41
};

static uint64_t sys_regs(char * buffer) {
    return copyRegisters(buffer);
}

// devuelve cuantos chars escribió
static uint64_t sys_write(uint64_t fd, const char * buffer, uint64_t count) {

    if (fd == STDOUT) { // que es para este tipo stdout?
        int pid = scheduler_get_current_pid();
        PCB * p = scheduler_get_process(pid);
        fd = p->write_fd;
    }

    if (fd == STDIN) { // no se puede escribir en STDIN
        return -1;
    }
    if (fd < FIRST_FREE_FD) {
        uint32_t color = fd_colors[fd];
        for (int i = 0; i < count; i++) {
            vdPutChar(buffer[i], color);
        }

        return count;
    }

    // es un pipe
    return write_pipe(fd, buffer, count);
}

// leo hasta count
static uint64_t sys_read(int fd, char * buffer, uint64_t count) {
    if (fd == STDIN) { // que es para este tipo stdin?
        int pid = scheduler_get_current_pid();
        PCB * p = scheduler_get_process(pid);
        fd = p->read_fd;
    }
    if (fd == STDOUT || fd == STDERR) { // no puede leer de ahi
        return -1;
    }

    if (fd == STDIN) {
        return read_keyboard_buffer(buffer, count);
    }

    // es un pipe
    return read_pipe(fd, buffer, count);

}

static void sys_date(uint8_t * buffer){
   get_date(buffer);
}

static void sys_time(uint8_t * buffer){
    get_time(buffer);
}

// limpia la shell y pone el cursor en el principio
static void sys_clear(){
    vdClear();
}

static void sys_increase_fontsize(){
    vdIncreaseTextSize();
}

static void sys_decrease_fontsize() {
    vdDecreaseTextSize();
}

// Ruido para el juego
static void sys_beep(uint32_t freq_hz, uint64_t duration){
    beep(freq_hz, duration);
}

// devuelve la info del tamaño de la pantalla
static void sys_screensize(uint32_t * width, uint32_t * height){
    *width = getScreenWidth();
    *height = getScreenHeight();
}

// info: [x_center, y_center, radius]
static void sys_circle(uint64_t fill, uint64_t * info, uint32_t color) {
    if (fill) {
        fill_circle(info[0], info[1], info[2], color);
    } else {
        draw_circle(info[0], info[1], info[2], color);
    }
}

// info: [x0, y0, x1, y1]
static void sys_rectangle(uint64_t fill, uint64_t * info, uint32_t color) {
    if (fill) {
        fill_rectangle(info[0], info[1], info[2], info[3], color);
    } else {
        draw_rectangle(info[0], info[1], info[2], info[3], color);
    }
}

// info: [x0, y0, x1, y1]
static void sys_draw_line(uint64_t * info, uint32_t color) {
    drawLine(info[0], info[1], info[2], info[3], color);
}

// info: [x0, y0, size]
static void sys_draw_string(const char * buf, uint64_t * info, uint32_t color) {
    drawString(buf, info[0], info[1], color, info[2]);
}

static void sys_speaker_start(uint32_t freq_hz){
    speaker_start_tone(freq_hz);
}

static void sys_speaker_stop(){
    speaker_off();
}

static void sys_textmode() {
    enableTextMode();
}

static void sys_videomode() {
    disableTextMode();
}

static void sys_putpixel(uint32_t hexColor, uint64_t x, uint64_t y) {
    putPixel(hexColor, x, y);
}

static uint64_t sys_key_status(char c) {
    return is_pressed_key(c);
}

static void sys_sleep(uint64_t miliseconds) {
    sleep(miliseconds);
}

static void sys_clear_input_buffer() {
    clear_buffer();
}

static uint64_t sys_ticks() {
    return ticks_elapsed();
}

// Memory management syscalls
static void * sys_malloc(size_t size) {
    return alloc_memory(get_kernel_memory_manager(), size);
}

static void sys_free(void * ptr) {
    free_memory(get_kernel_memory_manager(), ptr);
}

static MemStatus sys_memStatus(void) {
    return get_mem_status(get_kernel_memory_manager());
}

// ===================== Processes syscalls =====================

// Crea un proceso: reserva un PID libre y delega en el scheduler
static int64_t sys_create_process(void * entry, int argc, const char **argv, const char *name, int fds[2]) {
    if (entry == NULL || name == NULL) {
        return -1;
    }

    int new_pid = scheduler_add_process((process_entry_t)entry, argc, argv, name, fds);
    return new_pid;
}

// Termina el proceso actual con un status, POR AHORA NO USAMOS
static void sys_exit(int status) {
    scheduler_exit_process(status);
}

// Devuelve el PID del proceso en ejecución
static int64_t sys_getpid(void) {
    return (int64_t)scheduler_get_current_pid();
}

// Mata un proceso por PID
static int64_t sys_kill(int pid) {
    return (int64_t)scheduler_kill_process(pid);
}

// Bloquea un proceso por PID
static int64_t sys_block(int pid) {
    return (int64_t)scheduler_block_process(pid);
}

// Desbloquea un proceso por PID
static int64_t sys_unblock(int pid) {
    return (int64_t)scheduler_unblock_process(pid);
}

// Espera a que el proceso hijo 'pid' termine. Devuelve su código de salida si estaba terminado o 0 si bloqueó.
static int64_t sys_wait(int pid) {
    return (int64_t)scheduler_waitpid(pid);
}

static int64_t sys_nice(int64_t pid, int new_prio) {
    return scheduler_set_priority(pid, new_prio);
}

static void sys_yield() {
    scheduler_yield();
}

static void sys_print_processes() {
    scheduler_print_processes();
}

// SEMÁFOROS (API basada en nombre)
static int64_t sys_sem_open(const char *name, int value) {
    return (int64_t)sem_open((char *)name, value);
}
static void sys_sem_close(const char *name) {
    sem_close((char *)name);
}
static void sys_sem_wait(const char *name) {
    sem_wait((char *)name);
}
static void sys_sem_post(const char *name) {
    sem_post((char *)name);
}


static int sys_create_pipe(int fds[2]) {
   return create_pipe(fds);
}

static void sys_destroy_pipe(int fd) {
    destroy_pipe(fd);
}



