
#include <stdint.h>

#include "memoryManager.h"

#define PIPE_BUFFER_SIZE 1024
#define MAX_PIPES 64
#define SEM_NAME_SIZE 64
#define EOF -1

enum {
    STDIN = 0,
    STDOUT,
    STDERR,
    FIRS_FREE_FD
};


// deja en fd[0] el read_fd y en fd[1] el write_fd
// devuelve el id del pipe, -1 si no lo creo
int create_pipe(int fds[2]);

// devuelve cuantos bytes leyo, -1 si falla
int read_pipe(int fd, char * buf, int count);

// devuelve cuantos bytes escribio, -1 si falla
int write_pipe(int fd, char * buf, int count);

// libera los recursos del pipe
void destroy_pipe(int fd); 

