#include <videoDriver.h>
#include <font.h>
#include "lib.h"

#define abs(x) ((x) < 0 ? -(x) : (x))
#define ENABLED 1
#define DISABLED 0
#define BKG_COLOR 0x000000 // cuidado si lo cambiamos hay que cambiar el clear implementando un memset custom
#define MAX_SIZE 4

struct vbe_mode_info_structure {
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	
	// IMPORTA
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels

	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes;

	// IMPORTA
	uint8_t bpp;			// bits per pixel in this mode

	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;
	uint8_t reserved0;
 
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
 
	// IMPORTA
	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen

	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
} __attribute__ ((packed));

typedef struct vbe_mode_info_structure * VBEInfoPtr;

VBEInfoPtr VBE_mode_info = (VBEInfoPtr) 0x0000000000005C00;

static int isValid(uint64_t x, uint64_t y) {
    return x < VBE_mode_info->width && y < VBE_mode_info->height;
}

void putPixel(uint32_t hexColor, uint64_t x, uint64_t y) {
	// agregamos chequeo de que el pixel este dentro de los limites de la pantalla
	if (!isValid(x, y)) {
		return;
	}
    uint8_t * framebuffer = (uint8_t *) VBE_mode_info->framebuffer;
    uint64_t offset = (x * ((VBE_mode_info->bpp)/8)) + (y * VBE_mode_info->pitch);
    framebuffer[offset]     =  (hexColor) & 0xFF;
    framebuffer[offset+1]   =  (hexColor >> 8) & 0xFF;
    framebuffer[offset+2]   =  (hexColor >> 16) & 0xFF;
}

uint16_t getScreenHeight() {
	return VBE_mode_info->height;
}

uint16_t getScreenWidth() {
	return VBE_mode_info->width;
}

/*
    MODO TEXTO
*/

static int text_mode = DISABLED;

static uint64_t cursor_x;
static uint32_t cursor_y;
static uint8_t text_size = 1;

void enableTextMode() {
    if (text_mode) {
        return;
    }
    text_mode = ENABLED;
    vdClear();

}

void disableTextMode() {
    if (!text_mode) {
        return;
    }
    vdClear();
    text_mode = DISABLED;
}

void vdSetTextSize(uint8_t size) {
    text_size = size;
}

uint8_t vdGetTextSize() {
    return text_size;
}

static void scrollUp() {
    uint64_t line_height = text_size * FONT_HEIGHT;
    uint8_t * framebuffer = (uint8_t *) VBE_mode_info->framebuffer;
    
    // Usar memcpy para copiar cada línea completa
    // desde la segunda línea de texto hasta el final hacia arriba
    for (uint64_t src_y = line_height; src_y < VBE_mode_info->height; src_y++) {
        uint64_t dst_y = src_y - line_height;
        
        // Calcular offset de la línea fuente y destino
        uint64_t src_offset = src_y * VBE_mode_info->pitch;
        uint64_t dst_offset = dst_y * VBE_mode_info->pitch;
        
        memcpy(framebuffer + dst_offset, framebuffer + src_offset, VBE_mode_info->pitch);
    }
    
    // Limpiar la última línea
    uint64_t last_line_start = VBE_mode_info->height - line_height;
    fill_rectangle(0, last_line_start, VBE_mode_info->width, VBE_mode_info->height, BKG_COLOR);
}

void newLine() {
    cursor_x = 0;
    uint64_t step_y = text_size * FONT_HEIGHT;
    
    // Verificar si hay espacio para una línea más sin hacer scroll
    if (cursor_y + step_y < VBE_mode_info->height) { 
        cursor_y += step_y;
        fill_rectangle(cursor_x, cursor_y, VBE_mode_info->width, cursor_y + FONT_HEIGHT*text_size, BKG_COLOR);
    } else {
        scrollUp();
        cursor_y = VBE_mode_info->height - step_y; // Posicionar cursor en la última línea
    }
}

static void updateCursor() {
    if(!isValid(cursor_x + FONT_WIDTH*text_size, cursor_y + FONT_HEIGHT*text_size)) {
        newLine();
    }
}



static void moveCursorRight() {
    uint64_t step_x = FONT_WIDTH*text_size;
    if (cursor_x + 2*step_x <= VBE_mode_info->width) {
        cursor_x += step_x;
    } else {
        newLine();
    }
}

static void moveCursorLeft() {
    uint64_t step_x = FONT_WIDTH*text_size;
    if (cursor_x >= step_x) {
        cursor_x -= step_x;
    } else {
        uint64_t step_y = FONT_HEIGHT*text_size;
        if (cursor_y >= step_y) {
            cursor_y -= step_y;
            cursor_x = (VBE_mode_info->width / step_x - 1) * step_x;
        }
    }
}

static void deleteChar() {
    moveCursorLeft();
    for (int y = 0; y < FONT_HEIGHT*text_size; y++) {
        for (int x = 0; x < FONT_WIDTH*text_size; x++) {
            putPixel(BKG_COLOR, cursor_x + x, cursor_y + y);
        }
    }
}

void vdPutChar(uint8_t ch, uint32_t color) {
    if (!text_mode) {
        return;
    }
    switch (ch) {
        case '\b':
            deleteChar();
            break;
        case '\n':
            newLine();
            break;
        default:
            drawChar(ch, cursor_x, cursor_y, color, text_size);
            moveCursorRight();
            break;
	}
}

void vdPrint(const char * str, uint32_t color) {
    if (!text_mode) {
        return;
    }
    for (int i = 0; str[i] != 0; i++) {
        vdPutChar(str[i], color);
    }
}


void vdIncreaseTextSize() {
    if (text_size < MAX_SIZE && text_mode) {
        text_size++;
    }
    updateCursor();
}

void vdDecreaseTextSize() {
    if (text_size > 1 && text_mode) {
        text_size--;
    }
    updateCursor();
}

void vdClear() {
    if (!text_mode) {
        return;
    }
    // fill_rectangle(0, 0, VBE_mode_info->width, VBE_mode_info->width, BKG_COLOR);
    uint64_t length = getScreenHeight() * getScreenWidth() * VBE_mode_info->bpp / 8;
    memset64((void *) VBE_mode_info->framebuffer, 0, length); // hardcodeado que el background color es negro
    cursor_x = 0;
    cursor_y = 0;
}







/*
    MODO VIDEO
*/


void drawChar(uint8_t ch, uint64_t x, uint64_t y, uint32_t color, uint64_t size) {
    if (ch >= 128) {
        return;
    }
    
    if (size == 0) {
        size = 1; // aseguramos minimo size de 1
    }
    
    for (int i = 0; i < FONT_HEIGHT; i++) {
        char line = font[ch][i];
        for (int j = 0; j < FONT_WIDTH; j++) {
            if ((line << j) & 0x80) { 
                // dibuja un cuadrado de size × size pixels por cada bit
                for (uint64_t dy = 0; dy < size; dy++) {
                    for (uint64_t dx = 0; dx < size; dx++) {
                        putPixel(color, x + j*size + dx, y + i*size + dy);
                    }
                }
            }
        } 
    }
}

void drawString(const char * str, uint64_t x, uint64_t y, uint32_t color, uint64_t size) {
    if (size == 0) {
        size = 1; // aseguramos minimo size de 1
    }
    
    for (int i = 0; str[i] != 0; i++) {
        drawChar(str[i], x + (FONT_WIDTH*size)*i, y, color, size);
    }
}

void drawLine(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
	// algoritmo de Bresenham
    int64_t dx = abs((int64_t)x1 - (int64_t)x0);
    int64_t dy = abs((int64_t)y1 - (int64_t)y0);

    int64_t step_x = (x0 < x1) ? 1 : -1;
    int64_t step_y = (y0 < y1) ? 1 : -1;

    int64_t error = dx - dy;
	int done = 0;

    while (!done) {
        putPixel(color, x0, y0);  

		if ((x0 == x1 && y0 == y1) || !isValid(x0, y0)) { 
			done = 1;
		} else {
			int64_t error2 = 2 * error;
	
			if (error2 > -dy) {
				error -= dy;
				x0 += step_x;
			}
	
			if (error2 < dx) {
				error += dx;
				y0 += step_y;
			}
		}
    }


}


void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
	// checkeo que (x0, y0) sean esquina superior izquierda
	int dx = x1-x0;
	int dy = y1-y0;
	if (dx < 0 || dy < 0) {
		return;
	}
	
	drawLine(x0, y0, x1, y0, color);
	drawLine(x0, y0, x0, y1, color);
	drawLine(x1 ,y1, x0, y1, color);
	drawLine(x1, y1, x1, y0, color);

	
}

void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
	// checkeo que (x0, y0) sean esquina superior izquierda
	int dx = x1-x0;
	int dy = y1-y0;
	if (dx < 0 || dy < 0) {
		return;
	}
	
	for (int i = 0; i < dx; i++) {
		for (int j = 0; j < dy; j++) {
			putPixel(color, x0+i, y0+j);
		}
	}
	
}

void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color) {

    int64_t x = radius;
    int64_t y = 0;
    int64_t err = 0;

    while (x >= y) {
        putPixel(color, x_center + x, y_center + y);
        putPixel(color, x_center + y, y_center + x);
        putPixel(color, x_center - y, y_center + x);
        putPixel(color, x_center - x, y_center + y);
        putPixel(color, x_center - x, y_center - y);
        putPixel(color, x_center - y, y_center - x);
        putPixel(color, x_center + y, y_center - x);
        putPixel(color, x_center + x, y_center - y);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color) {
    uint64_t x0 = (x_center >= radius) ? x_center - radius : 0;
    uint64_t y0 = (y_center >= radius) ? y_center - radius : 0;
    uint64_t x1 = x_center + radius;
    uint64_t y1 = y_center + radius;

    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            int dx = x - x_center;
            int dy = y - y_center;
            if (dx*dx + dy*dy <= radius*radius) { // si esta adentro del circulo
                putPixel(color, x, y);
            }
        }
    }
}
