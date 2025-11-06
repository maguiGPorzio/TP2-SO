#include "usrlib.h"

int unblock(int argc, char *argv[]) {
    if (argc != 1) {
        printf("Invalid arguments. Usage: unblock <pid>\n");
        return ERROR;
    }
    
    int pid = satoi(argv[0]);
    if (pid < 0) {
        printf("Invalid PID: %s\n", argv[0]);
        return ERROR;
    }

    int result = sys_unblock(pid);
    if (result != OK) {
        printf("Failed to unblock process with PID %d\n", pid);
        return ERROR;
    }
    printf("Process with PID %d unblocked successfully.\n", pid);

    return OK;
}