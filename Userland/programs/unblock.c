#include "usrlib.h"

int unblock(int argc, char *argv[]) {
    if (argc != 1) {
        printf("Invalid arguments. Usage: unblock <pid>\n");
        return -1;
    }
    
    int pid = satoi(argv[0]);
    if (pid < 0) {
        printf("Invalid PID: %s\n", argv[0]);
        return -1;
    }

    process_info_t buffer[MAX_PROCESSES];
    int count = sys_processes_info(buffer, MAX_PROCESSES);
    
    if (count <= 0) {
        printf("Error: Could not retrieve process information.\n");
        return -1;
    }
    
    int found = 0;
    process_info_t *target = NULL;
    
    for (int i = 0; i < count; i++) {
        if (buffer[i].pid == pid) {
            target = &buffer[i];
            found = 1;
            break;
        }
    }
    
    if (!found) {
        printf("Error: Process with PID %u not found.\n", (unsigned)pid);
        return -1;
    }
    
    if (target->status == PS_BLOCKED) {
        int ret = sys_unblock(pid);
        if (ret < 0) {
            printf("Error: Failed to unblock process %u.\n", (unsigned)pid);
            return -1;
        }
        printf("Process %u (%s) changed from BLOCKED to READY.\n", (unsigned)pid, target->name);

    } else if (target->status == PS_RUNNING || target->status == PS_READY) {
        printf("Process %u (%s) is already %s.\n", (unsigned)pid, target->name, "READY");

    } else {
        printf("Error: Process %u is %s, cannot toggle state.\n", (unsigned)pid, target->status == PS_TERMINATED ? "TERMINATED" : "UNKNOWN");
        return -1;
    }
    
    return 0;
}