#include "usrlib.h"

void draw_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
    uint64_t info[] = {x0, y0, x1, y1};
    sys_rectangle(0, info, color);
}

void fill_rectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
    uint64_t info[] = {x0, y0, x1, y1};
    sys_rectangle(1, info, color);
}

void draw_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color) {
    uint64_t info[] = {x_center, y_center, radius};
    sys_circle(0, info, color);
}

void fill_circle(uint64_t x_center, uint64_t y_center, uint64_t radius, uint32_t color) {
    uint64_t info[] = {x_center, y_center, radius};
    sys_circle(1, info, color);
}

void draw_string(char * str, uint64_t x, uint64_t y, uint64_t size, uint32_t color) {
    uint64_t info[] = {x, y, size};
    sys_draw_string(str, info, color);
}

void draw_line(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1, uint32_t color) {
    uint64_t info[] = {x0, y0, x1, y1};
    sys_line(info, color);
}