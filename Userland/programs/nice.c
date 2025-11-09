// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int nice_main(int argc, char *argv[]){
    if(argc != 2){
        print_err("Usage: nice <pid> <new_priority>\n");
        return ERROR;
    }

    int pid = satoi(argv[0]);
    int new_prio = satoi(argv[1]);
    
    int result = sys_nice((int64_t)pid, new_prio);
    if(result == ERROR){
        printf("Failed to change priority. Check PID and priority range (%d-%d).\n", MIN_PRIORITY, MAX_PRIORITY);
        return ERROR;
    }

    printf("Changed priority of process %d to %d\n", pid, new_prio);
    
    return OK;
    
}