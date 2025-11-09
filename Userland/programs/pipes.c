// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int pipes_main(int argc, char *argv[]) {
    pipe_info_t pipes[MAX_PIPES];
    int count = sys_pipes_info(pipes, MAX_PIPES);
    
    if (count < 0) {
        print_err("Failed to get pipes info\n");
        return ERROR;
    }
    
    if (count == 0) {
        print("No active pipes\n");
        return OK;
    }
    
    print("ID   NAME                          FD_R  FD_W  READERS  WRITERS  BUFFERED\n");
    print("----------------------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        pipe_info_t *p = &pipes[i];
        
        // ID
        printf("%d    ", p->id);
        
        // Nombre (anonymous si está vacío)
        if (p->name[0] == '\0') {
            print("[anonymous]");
            // Rellenar espacios (anonymous = 11 chars)
            for (int j = 11; j < 30; j++) {
                putchar(' ');
            }
        } else {
            print(p->name);
            // Rellenar espacios para alinear (nombre max 30 chars)
            int name_len = strlen(p->name);
            for (int j = name_len; j < 30; j++) {
                putchar(' ');
            }
        }
        
        // FDs
        printf("%d     %d     ", p->read_fd, p->write_fd);
        
        // Contadores
        printf("%d        %d        ", p->readers, p->writers);
        
        // Buffered
        printf("%d/%d\n", p->buffered, 1024); // PIPE_BUFFER_SIZE = 1024
    }
    
    putchar('\n');
    printf("Total: %d pipe", count);
    if (count != 1) {
        putchar('s');
    }
    putchar('\n');
    
    return OK;
}
