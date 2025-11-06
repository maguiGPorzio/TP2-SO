#include "usrlib.h"

int nice_main(int argc, char *argv[]){
    if(argc != 2){
        print_err("Usage: nice <pid> <new_priority>\n");
        return -1;
    }

    int pid = satoi(argv[0]);
    int new_prio = satoi(argv[1]);
    
    int result = sys_nice((int64_t)pid, new_prio);
    if(result == -1){
        printf("Failed to change priority. Check PID and priority range (%d-%d).\n", MIN_PRIORITY, MAX_PRIORITY);
        return -1;
    }

    printf("Changed priority of process %d to %d\n", pid, new_prio);
    
    return 0;
    
}