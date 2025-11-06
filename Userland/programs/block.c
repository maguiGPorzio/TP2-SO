#include "usrlib.h"

int block(int argc, char *argv[]) {
    if (argc != 1) {
        print("Invalid arguments. Usage: block <pid>\n");
        return -1;
    }
    
    int pid = satoi(argv[0]);
    if (pid <= 0) {
        printf("Invalid PID: %s\n", argv[0]);
        return -1;
    }

    int result = sys_block(pid);
    if (result != 0) {
        printf("Failed to block process with PID %d\n", pid);
        return -1;
    }
    printf("Process with PID %d blocked successfully.\n", pid);
    
    return 0;
}