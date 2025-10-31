
#include <stdint.h>
#include <stdbool.h>
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
bool create_pipe(int fds[2]);

int write_pipe(int fd, char * buf, int count);

int read_pipe(int fd, char * buf, int count);

void destroy_pipe(int fd);

