#include "usrlib.h"
#include "syscalls.h"

// loop: Imprime su ID con un saludo cada una determinada cantidad de segundos
int loop_main() {
    int pid = sys_getpid();
    while (1) {
        print("Proceso ");
        printf("%d", pid);
        print(": Hola desde el loop!\n");
        sys_sleep(2000); // duerme 2 segundos
    }
}