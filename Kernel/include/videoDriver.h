#ifndef _VIDEO_DRIVER_H_
#define _VIDEO_DRIVER_H_

#include <stdint.h>

// dibuja un pixel en pantalla
void put_pixel(uint32_t hex_color, uint64_t x, uint64_t y);
uint16_t get_screen_height();
uint16_t get_screen_width();

/*  FUNCIONES DE MODO TEXTO  */
void enable_text_mode();
void disable_text_mode();
void vd_set_text_size(uint8_t size);
uint8_t vd_get_text_size();
void vd_increase_text_size();
void vd_decrease_text_size();
void vd_put_char(uint8_t ch, uint32_t color);
void vd_print(const char * str, uint32_t color);
void vd_clear();
void newline();

/*  FUNCIONES DE DIBUJO */

// x,y son esquina superior izquierda
void vd_draw_char(uint8_t ch, uint64_t x, uint64_t y, uint32_t color, uint64_t size);
void vd_draw_string(const char * str, uint64_t x, uint64_t y, uint32_t color, uint64_t size);
// dibuja una linea desde x0,y0 hasta x1,y1
void vd_draw_line(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
// x0,y0 esquina superior izquierda. x1,y1 esquina inferior derecha
void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);
void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);


#endif // _VIDEO_DRIVER_H_