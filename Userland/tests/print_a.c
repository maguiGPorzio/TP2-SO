#include "../include/usrlib.h"

extern int proc_print_b(int argc, char **argv);
// Proceso de prueba: imprime 'a' y retorna 0
int proc_print_a(int argc, char **argv) {
    const char *args[] = { "b" };
    sys_create_process(proc_print_b, 1, args, "proc_b");
    while(1) {
        printf(argv[0]);
    }
}