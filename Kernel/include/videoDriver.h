#ifndef _VIDEO_DRIVER_H_
#define _VIDEO_DRIVER_H_

#include <stdint.h>

// dibuja un pixel en pantalla
void putPixel(uint32_t hexColor, uint64_t x, uint64_t y);
uint16_t getScreenHeight();
uint16_t getScreenWidth();

/*  FUNCIONES DE MODO TEXTO  */
void enableTextMode();
void disableTextMode();
void vdSetTextSize(uint8_t size);
uint8_t vdGetTextSize();
void vdIncreaseTextSize();
void vdDecreaseTextSize();
void vdPutChar(uint8_t ch, uint32_t color);
void vd_print(const char * str, uint32_t color);
void vdClear();
void newLine();

/*  FUNCIONES DE DIBUJO */

// x,y son esquina superior izquierda
void drawChar(uint8_t ch, uint64_t x, uint64_t y, uint32_t color, uint64_t size);
void drawString(const char * str, uint64_t x, uint64_t y, uint32_t color, uint64_t size);
// dibuja una linea desde x0,y0 hasta x1,y1
void drawLine(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
// x0,y0 esquina superior izquierda. x1,y1 esquina inferior derecha
void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color);
void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);
void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color);


#endif // _VIDEO_DRIVER_H_