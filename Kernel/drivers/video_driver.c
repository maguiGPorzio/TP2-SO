// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#include "lib.h"
#include <font.h>
#include <video_driver.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define ENABLED 1
#define DISABLED 0
#define BKG_COLOR                                                              \
  0x000000 // cuidado si lo cambiamos hay que cambiar el clear implementando un
           // memset custom
#define MAX_SIZE 4

struct vbe_mode_info_structure {
  uint16_t
      attributes; // deprecated, only bit 7 should be of interest to you, and it
                  // indicates the mode supports a linear frame buffer.
  uint8_t window_a;     // deprecated
  uint8_t window_b;     // deprecated
  uint16_t granularity; // deprecated; used while calculating bank numbers
  uint16_t window_size;
  uint16_t segment_a;
  uint16_t segment_b;
  uint32_t win_func_ptr; // deprecated; used to switch banks from protected mode
                         // without returning to real mode

  uint16_t pitch;  // number of bytes per horizontal line
  uint16_t width;  // width in pixels
  uint16_t height; // height in pixels

  uint8_t w_char; // unused...
  uint8_t y_char; // ...
  uint8_t planes;

  uint8_t bpp; // bits per pixel in this mode

  uint8_t banks; // deprecated; total number of banks in this mode
  uint8_t memory_model;
  uint8_t bank_size; // deprecated; size of a bank, almost always 64 KB but may
                     // be 16 KB...
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

  uint32_t framebuffer; // physical address of the linear frame buffer; write
                        // here to draw to the screen

  uint32_t off_screen_mem_off;
  uint16_t off_screen_mem_size; // size of memory in the framebuffer but not
                                // being displayed on the screen
  uint8_t reserved1[206];
} __attribute__((packed));

typedef struct vbe_mode_info_structure *VBEInfoPtr;

VBEInfoPtr VBE_mode_info = (VBEInfoPtr)0x0000000000005C00;

static int is_within_screen_bounds(uint64_t x, uint64_t y) {
  return x < VBE_mode_info->width && y < VBE_mode_info->height;
}

void put_pixel(uint32_t hex_color, uint64_t x, uint64_t y) {
  if (!is_within_screen_bounds(x, y)) {
    return;
  }
  uint8_t *framebuffer = (uint8_t *)(uintptr_t)VBE_mode_info->framebuffer;
  uint64_t offset =
      (x * ((VBE_mode_info->bpp) / 8)) + (y * VBE_mode_info->pitch);
  framebuffer[offset] = (hex_color) & 0xFF;
  framebuffer[offset + 1] = (hex_color >> 8) & 0xFF;
  framebuffer[offset + 2] = (hex_color >> 16) & 0xFF;
}

uint16_t get_screen_height() { return VBE_mode_info->height; }

uint16_t get_screen_width() { return VBE_mode_info->width; }

/*
    MODO TEXTO
*/

static int text_mode = DISABLED;

static uint64_t cursor_x;
static uint32_t cursor_y;
static uint8_t text_size = 1;

void enable_text_mode() {
  if (text_mode) {
    return;
  }
  text_mode = ENABLED;
  vd_clear();
}

void disable_text_mode() {
  if (!text_mode) {
    return;
  }
  vd_clear();
  text_mode = DISABLED;
}

void vd_set_text_size(uint8_t size) { text_size = size; }

uint8_t vd_get_text_size() { return text_size; }

static void scroll_up() {
  uint64_t line_height = text_size * FONT_HEIGHT;
  uint8_t *framebuffer = (uint8_t *)(uintptr_t)VBE_mode_info->framebuffer;

  // Usar memcpy para copiar cada línea completa
  // desde la segunda línea de texto hasta el final hacia arriba
  for (uint64_t src_y = line_height; src_y < VBE_mode_info->height; src_y++) {
    uint64_t dst_y = src_y - line_height;

    // Calcular offset de la línea fuente y destino
    uint64_t src_offset = src_y * VBE_mode_info->pitch;
    uint64_t dst_offset = dst_y * VBE_mode_info->pitch;

    memcpy(framebuffer + dst_offset, framebuffer + src_offset,
           VBE_mode_info->pitch);
  }

  // Limpiar la última línea
  uint64_t last_line_start = VBE_mode_info->height - line_height;
  fill_rectangle(0, last_line_start, VBE_mode_info->width,
                 VBE_mode_info->height, BKG_COLOR);
}

void newline() {
  cursor_x = 0;
  uint64_t step_y = text_size * FONT_HEIGHT;

  // Verificar si hay espacio para una línea más sin hacer scroll
  if (cursor_y + step_y < VBE_mode_info->height) {
    cursor_y += step_y;
    fill_rectangle(cursor_x, cursor_y, VBE_mode_info->width,
                   cursor_y + FONT_HEIGHT * text_size, BKG_COLOR);
  } else {
    scroll_up();
    cursor_y =
        VBE_mode_info->height - step_y; // Posicionar cursor en la última línea
  }
}

static void update_cursor() {
  if (!is_within_screen_bounds(cursor_x + FONT_WIDTH * text_size,
                               cursor_y + FONT_HEIGHT * text_size)) {
    newline();
  }
}

static void move_cursor_right() {
  uint64_t step_x = FONT_WIDTH * text_size;
  if (cursor_x + 2 * step_x <= VBE_mode_info->width) {
    cursor_x += step_x;
  } else {
    newline();
  }
}

static void move_cursor_left() {
  uint64_t step_x = FONT_WIDTH * text_size;
  if (cursor_x >= step_x) {
    cursor_x -= step_x;
  } else {
    uint64_t step_y = FONT_HEIGHT * text_size;
    if (cursor_y >= step_y) {
      cursor_y -= step_y;
      cursor_x = (VBE_mode_info->width / step_x - 1) * step_x;
    }
  }
}

static void delete_char() {
  move_cursor_left();
  for (int y = 0; y < FONT_HEIGHT * text_size; y++) {
    for (int x = 0; x < FONT_WIDTH * text_size; x++) {
      put_pixel(BKG_COLOR, cursor_x + x, cursor_y + y);
    }
  }
}

void vd_put_char(uint8_t ch, uint32_t color) {
  if (!text_mode) {
    return;
  }
  switch (ch) {
  case '\b':
    delete_char();
    break;
  case '\n':
    newline();
    break;
  default:
    vd_draw_char(ch, cursor_x, cursor_y, color, text_size);
    move_cursor_right();
    break;
  }
}

void vd_print(const char *str, uint32_t color) {
  if (!text_mode) {
    return;
  }
  for (int i = 0; str[i] != 0; i++) {
    vd_put_char(str[i], color);
  }
}

void vd_increase_text_size() {
  if (text_size < MAX_SIZE && text_mode) {
    text_size++;
  }
  update_cursor();
}

void vd_decrease_text_size() {
  if (text_size > 1 && text_mode) {
    text_size--;
  }
  update_cursor();
}

void vd_clear() {
  if (!text_mode) {
    return;
  }
  uint64_t length =
      get_screen_height() * get_screen_width() * VBE_mode_info->bpp / 8;
  memset64((void *)(uintptr_t)VBE_mode_info->framebuffer, 0,
           length); // hardcodeado que el background color es negro
  cursor_x = 0;
  cursor_y = 0;
}

/*
    MODO VIDEO
*/

void vd_draw_char(uint8_t ch, uint64_t x, uint64_t y, uint32_t color,
                  uint64_t size) {
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
            put_pixel(color, x + j * size + dx, y + i * size + dy);
          }
        }
      }
    }
  }
}

void vd_draw_string(const char *str, uint64_t x, uint64_t y, uint32_t color,
                    uint64_t size) {
  if (size == 0) {
    size = 1; // aseguramos minimo size de 1
  }

  for (int i = 0; str[i] != 0; i++) {
    vd_draw_char(str[i], x + (FONT_WIDTH * size) * i, y, color, size);
  }
}

void vd_draw_line(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1,
                  uint32_t color) {
  // algoritmo de Bresenham
  int64_t dx = ABS((int64_t)x1 - (int64_t)x0);
  int64_t dy = ABS((int64_t)y1 - (int64_t)y0);

  int64_t step_x = (x0 < x1) ? 1 : -1;
  int64_t step_y = (y0 < y1) ? 1 : -1;

  int64_t error = dx - dy;
  int done = 0;

  while (!done) {
    put_pixel(color, x0, y0);

    if ((x0 == x1 && y0 == y1) || !is_within_screen_bounds(x0, y0)) {
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

void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1,
                    uint32_t color) {
  // checkeo que (x0, y0) sean esquina superior izquierda
  int dx = x1 - x0;
  int dy = y1 - y0;
  if (dx < 0 || dy < 0) {
    return;
  }

  vd_draw_line(x0, y0, x1, y0, color);
  vd_draw_line(x0, y0, x0, y1, color);
  vd_draw_line(x1, y1, x0, y1, color);
  vd_draw_line(x1, y1, x1, y0, color);
}

void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1,
                    uint32_t color) {
  // checkeo que (x0, y0) sean esquina superior izquierda
  int dx = x1 - x0;
  int dy = y1 - y0;
  if (dx < 0 || dy < 0) {
    return;
  }

  for (int i = 0; i < dx; i++) {
    for (int j = 0; j < dy; j++) {
      put_pixel(color, x0 + i, y0 + j);
    }
  }
}

void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius,
                 uint32_t color) {

  int64_t x = radius;
  int64_t y = 0;
  int64_t err = 0;

  while (x >= y) {
    put_pixel(color, x_center + x, y_center + y);
    put_pixel(color, x_center + y, y_center + x);
    put_pixel(color, x_center - y, y_center + x);
    put_pixel(color, x_center - x, y_center + y);
    put_pixel(color, x_center - x, y_center - y);
    put_pixel(color, x_center - y, y_center - x);
    put_pixel(color, x_center + y, y_center - x);
    put_pixel(color, x_center + x, y_center - y);

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

void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius,
                 uint32_t color) {
  uint64_t x0 = (x_center >= radius) ? x_center - radius : 0;
  uint64_t y0 = (y_center >= radius) ? y_center - radius : 0;
  uint64_t x1 = x_center + radius;
  uint64_t y1 = y_center + radius;

  for (int x = x0; x <= x1; x++) {
    for (int y = y0; y <= y1; y++) {
      int dx = x - x_center;
      int dy = y - y_center;
      if (dx * dx + dy * dy <= radius * radius) { // si esta adentro del circulo
        put_pixel(color, x, y);
      }
    }
  }
}
