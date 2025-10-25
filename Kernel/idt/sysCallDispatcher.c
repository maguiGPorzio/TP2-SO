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

    // syscalls de procesos (scheduler)
    &sys_create_process,     // 26
    &sys_exit_current,       // 27
    &sys_getpid,             // 28
    &sys_kill,               // 29
    &sys_block,              // 30
    0,                       // 31 reservado (no-op en ASM)
    &sys_unblock,            // 32
    &sys_wait,               // 33
    &sys_yield               // 34
};

static uint64_t sys_regs(char * buffer){
    return copyRegisters(buffer);
}

// devuelve cuantos chars escribió
static uint64_t sys_write(uint64_t fd, const char * buffer, uint64_t count) {
    if (fd != STDOUT && fd != STDERR) {
        return 0;
    }
    uint32_t color = fd == STDERR ? 0xff0000 : 0xffffff;
    for (int i = 0; i < count; i++) {
        vdPutChar(buffer[i], color);
    }
    
    return count;
}

// leo hasta count
static uint64_t sys_read(char * buffer, uint64_t count) {
    return read_keyboard_buffer(buffer, count);
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
        fillCircle(info[0], info[1], info[2], color);
    } else {
        drawCircle(info[0], info[1], info[2], color);
    }
}

// info: [x0, y0, x1, y1]
static void sys_rectangle(uint64_t fill, uint64_t * info, uint32_t color) {
    if (fill) {
        fillRectangle(info[0], info[1], info[2], info[3], color);
    } else {
        drawRectangle(info[0], info[1], info[2], info[3], color);
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
    return isPressedKey(c);
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
    return allocMemory(getKernelMemoryManager(), size);
}

static void sys_free(void * ptr) {
    freeMemory(getKernelMemoryManager(), ptr);
}

static MemStatus sys_memStatus(void) {
    return getMemStatus(getKernelMemoryManager());
}

// ===================== Processes syscalls =====================
// Encuentra el primer PID libre consultando al scheduler
static int find_free_pid(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler_get_process(i) == NULL) {
            return i;
        }
    }
    return -1;
}

// Crea un proceso: reserva un PID libre y delega en el scheduler
static int64_t sys_create_process(void * entry, int argc, const char **argv, const char *name) {
    if (entry == NULL || name == NULL) {
        return -1;
    }
    int pid = find_free_pid();
    if (pid < 0) {
        return -1;
    }
    int rc = scheduler_add_process(pid, (process_entry_t)entry, argc, argv, name);
    return (rc == 0) ? (int64_t)pid : -1;
}

// Termina el proceso actual con un status
static void sys_exit_current(int status) {
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
    return (int64_t)scheduler_wait(pid);
}

// Cede la CPU voluntariamente: reencola el proceso actual y fuerza cambio inmediato
extern void *current_kernel_rsp;
extern void *switch_to_rsp;
static void sys_yield(void) {
    // Marcar intención de replanificar y seleccionar próximo
    scheduler_yield();
    void *next_rsp = schedule(current_kernel_rsp);
    switch_to_rsp = next_rsp; // handler hará el cambio de pila antes de iretq
}

