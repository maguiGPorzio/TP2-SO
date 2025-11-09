#include "usrlib.h"

int unblock_main(int argc, char *argv[]) {
    if (argc == 0) {
        printf("Usage: unblock <pid1> [pid2] [pid3] ...\n");
        return ERROR;
    }
    
    int errors = 0;
    
    for (int i = 0; i < argc; i++) {
        int pid = satoi(argv[i]);
        
        if (pid < 0) {
            printf("Invalid PID: %s\n", argv[i]);
            errors++;
            continue;
        }
        
        int result = sys_unblock(pid);
        
        if (result == OK) {
            printf("Process %d unblocked successfully\n", pid);
        } else {
            printf("Error: Failed to unblock process %d\n", pid);
            errors++;
        }
    }
    
    return (errors == 0) ? OK : ERROR;
}