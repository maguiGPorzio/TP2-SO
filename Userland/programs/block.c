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

    process_info_t buffer[MAX_PROCESSES];
    int count = sys_processes_info(buffer, MAX_PROCESSES);
    
    if (count <= 0) {
        print("Error: Could not retrieve process information.\n");
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
    
    if (target->status == PS_READY) {
        printf(target->name);
        int ret = sys_block(pid);
        if (ret < 0) {
            printf("Error: Failed to block process %u.\n", (unsigned)pid);
            return -1;
        }
        printf("Process %u (%s) changed from READY to BLOCKED.\n", (unsigned)pid, target->name);
        
    } else if (target->status == PS_BLOCKED) {
        printf("Process %u (%s) is already BLOCKED.\n", (unsigned)pid, target->name);
        
    } else {
        const char *status_name;
        switch (target->status) {
            case PS_RUNNING:
                status_name = "RUNNING";
                break;
            case PS_TERMINATED:
                status_name = "TERMINATED";
                break;
            default:
                status_name = "UNKNOWN";
                break;
        }
        printf("Error: Process %u is %s, cannot change state.\n", (unsigned)pid, status_name);
        return -1;
    }
    
    return 0;
}