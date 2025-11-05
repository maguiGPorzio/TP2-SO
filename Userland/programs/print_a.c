#include "usrlib.h"

int print_a_main() {
    while (1) {
        print("a");
        sys_sleep(1000); // Pequeña pausa para evitar saturar la salida
    }
}