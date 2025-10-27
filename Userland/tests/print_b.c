#include "../include/usrlib.h"

// Proceso de prueba: imprime 'a' y retorna 0
int proc_print_b(int argc, char **argv) {
    for (int i = 0; i < 10000; i++) {
        putchar('b');
    }
    return 0;
}