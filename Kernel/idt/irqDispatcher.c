#include "time.h"
#include <stdint.h>
#include "keyboard.h"

static uint64_t int_20(uint64_t rsp);
static void int_21();

uint64_t irqDispatcher(uint64_t irq, uint64_t rsp) {
    switch (irq) {
        case 0:
            rsp = int_20(rsp);
            break;
        case 1:
            int_21();
            break;
    }
    return rsp;
}

uint64_t int_20(uint64_t rsp) {
	return timer_handler(rsp);
}

void int_21() {
	handle_pressed_key();
}