#include "usrlib.h"

int block(int argc, char *argv[]) {
    if (argc != 1) {
        print("Invalid arguments. Usage: block <pid>\n");
        return ERROR;
    }
    
    int pid = satoi(argv[0]);
    if (pid <= 0) {
        printf("Invalid PID: %s\n", argv[0]);
        return ERROR;
    }

    int result = sys_block(pid);
    if (result != OK) {
        printf("Failed to block process with PID %d\n", pid);
        return ERROR;
    }
    printf("Process with PID %d blocked successfully.\n", pid);
    
    return OK;
}