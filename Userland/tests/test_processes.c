#include "../include/usrlib.h"

// Proceso de prueba: imprime 'a' y retorna 0
int proc_print_a(int argc, char **argv) {
    (void)argc; (void)argv;
    putchar('a');
    putchar('\n');
    return 0;
}
