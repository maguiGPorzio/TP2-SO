#include "usrlib.h"

int kill_main(int argc, char * argv[]) {
    if (argc == 0) {
        print_err("Error: kill requires a PID\n");
        print_err("Usage: kill <pid>\n");
        return;
    }
    
    if (argc > 1) {
        print_err("Error: kill takes exactly one argument\n");
        print_err("Usage: kill <pid>\n");
        return;
    }
    
    // Parsear el PID desde el string usando satoi
    char *pid_str = argv[0];
    int pid = satoi(pid_str);
    
    // Llamar la syscall para matar el proceso
    int result = sys_kill(pid);

    // Imprimir mensajes específicos según el código de error
    switch (result) {
        case -1:
            print_err("[ERROR] Scheduler not initialized\n");
            break;
        case -2:
            print_err("[ERROR] Invalid PID\n");
            break;
        case -3:
            print_err("[ERROR] No process found with that PID\n");
            break;
        case -4:
            print_err("[ERROR] Cannot kill process: protected process (init or shell)\n");
            break;
    }
}