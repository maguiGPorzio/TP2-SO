#include "usrlib.h"

int print_b_main() {
    while (1) {
        print("b");
        sys_sleep(1000); // Pequeña pausa para evitar saturar la salida
    }
}