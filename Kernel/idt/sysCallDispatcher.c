 #include <stdint.h>
#include "videoDriver.h"
#include "time.h"
#include "keyboard.h"
#include "sound.h"
#include "sysCallDispatcher.h"

#define MIN_CHAR 0
#define MAX_CHAR 256

void * syscalls[] = {
    &sys_regs,
    &sys_time,
    &sys_date,
    &sys_read,
    &sys_write,
    &sys_increase_fontsize,
    &sys_decrease_fontsize,
    &sys_beep,
    &sys_screensize,
    &sys_circle,
    &sys_rectangle,
    &sys_draw_line,
    &sys_draw_string,
    &sys_clear,
    &sys_speaker_start,
    &sys_speaker_stop,
    &sys_textmode,
    &sys_videomode,
    &sys_putpixel,
    &sys_key_status,
    &sys_sleep,
    &sys_clear_input_buffer,
    &sys_ticks
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