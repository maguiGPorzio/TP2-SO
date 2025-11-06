#include "usrlib.h"

int ps_main(int argc, char *argv[]) {
    process_info_t processes[MAX_PROCESSES];
    int count = sys_processes_info(processes, MAX_PROCESSES);
    
    if (count < 0) {
        print_err("Failed to get processes info\n");
        return 1;
    }
    
    print("PID  NAME                 STATUS       PRIO  PPID  FD_R  FD_W  STACK_BASE    STACK_PTR\n");
    print("--------------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        process_info_t *p = &processes[i];
        
        // PID
        printf("%d    ", p->pid);
        
        // Nombre
        print(p->name);
        
        // Rellenar espacios para alinear (nombre max 20 chars)
        int name_len = strlen(p->name);
        for (int j = name_len; j < 21; j++) {
            putchar(' ');
        }
        
        // Status
        if (p->status == PS_READY) {
            print("READY        ");
        } else if (p->status == PS_RUNNING) {
            print("RUNNING      ");
        } else if (p->status == PS_BLOCKED) {
            print("BLOCKED      ");
        } else if (p->status == PS_TERMINATED) {
            print("TERMINATED   ");
        } else {
            print("UNKNOWN      ");
        }
        
        // Prioridad
        printf("%d     ", p->priority);
        
        if (p->parent_pid < 0) {
            print("-     "); // no parent pid
        } else {
            printf("%d     ", p->parent_pid);
        }
        
        // FDs
        printf("%d     %d     ", p->read_fd, p->write_fd);
        
        // Stack pointers en hex
        printf("0x%x      0x%x\n", p->stack_base, p->stack_pointer);
    }
    
    putchar(EOF);
    return OK;
}
