#include "pipes.h"

typedef struct pipe {
    char buffer[PIPE_BUFFER_SIZE]; // buffer circular
    int read_fd;
    int write_fd;
    int read_idx;
    int write_idx;
    char sem_read[SEM_NAME_SIZE];
    char sem_write[SEM_NAME_SIZE];
} pipe_t;

static pipe_t * pipes[MAX_PIPES] = {NULL};
static uint32_t  next_free_fd = FIRS_FREE_FD;

static int get_free_idx() {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i]) {
            return i;
        }
    }

    return -1;
}


bool create_pipe(int fds[2]) {
    int idx = get_free_idx();
    if (idx < 0) {
        return false;
    }

    MemoryManagerADT mm = get_kernel_memory_manager();
    pipe_t * pipe = alloc_memory(mm, sizeof(pipe_t));
    if (pipe == NULL) {
        return false;
    }

    pipes[idx] = pipe;

    pipe->read_idx = 0;
    pipe->write_idx = 0;
    pipe->read_fd = next_free_fd++;
    fds[0] = pipe->read_fd;
    pipe->write_fd = next_free_fd++;
    fds[1] = pipe->write_fd;

    // TODO: crear los semaforos

     



    
}

int write_pipe(int fd, char * buf, int count);

int read_pipe(int fd, char * buf, int count);

void destroy_pipe(int fd);