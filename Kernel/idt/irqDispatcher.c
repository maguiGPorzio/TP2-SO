#include "time.h"
#include <stdint.h>
#include "keyboard.h"


#define MAX_IRQ 2

static void int_20();
static void int_21();

// Array de punteros a función para las interrupciones
static void (*irq_handlers[])(void) = {
    int_20,  // IRQ 0
    int_21   // IRQ 1
};

void irqDispatcher(uint64_t irq) {
    if (irq < MAX_IRQ) {
        irq_handlers[irq]();
    }
    return;
}

void int_20() {
	timer_handler();
}

void int_21() {
	handlePressedKey();
}