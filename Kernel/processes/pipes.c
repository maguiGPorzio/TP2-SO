#include "pipes.h"
#include "lib.h"
#include "synchro.h"

typedef struct pipe {
    char buffer[PIPE_BUFFER_SIZE]; // buffer circular
    int read_fd;
    int write_fd;
    int read_idx;
    int write_idx;
    char read_sem[SEM_NAME_SIZE];
    char write_sem[SEM_NAME_SIZE];
} pipe_t;

// TODO: cambiar la implementacion a usar queues para los file descriptors disponibles y los index disponibles, ya que si hago eso voy a poder tener un array para medio que hashear a que index corresponde cada fd

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

static int get_pipe_from_fd(int fd) {
    if (fd < FIRS_FREE_FD || fd >= next_free_fd) {
        return -1;
    }

    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i] != NULL && (pipes[i]->read_fd == fd || pipes[i]->write_fd == fd)) {
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

    // semaforo para leer
    num_to_str(pipe->read_fd, pipe->read_sem);
    strcat(pipe->read_sem, "_read");
    if (sem_open(pipe->read_sem, 0) < 0) {
        pipes[idx] = NULL;
        free_memory(mm, pipe);
        return false; 
    }
    
    // semaforo para escribir
    num_to_str(pipe->write_fd, pipe->write_sem);
    strcat(pipe->write_sem, "_write");
    if (sem_open(pipe->write_sem, PIPE_BUFFER_SIZE) < 0) {
        sem_close(pipe->read_sem);
        pipes[idx] = NULL;
        free_memory(mm, pipe);
        return false;
    }

    return true;
    
}

int read_pipe(int fd, char * buf, int count) {
    int idx = get_pipe_from_fd(fd);
    if (idx < 0) {
        return -1;
    }

    pipe_t * pipe = pipes[idx];

    if (fd == pipe->write_fd) { // no se puede leer en el extremo de escritura
        return -1;
    }

    for (int i = 0; i < count; i++) {
        sem_wait(pipe->read_sem);
        buf[i] = pipe->buffer[pipe->read_idx];
        pipe->read_idx = (pipe->read_idx + 1) % PIPE_BUFFER_SIZE;
        sem_post(pipe->write_sem);
    }

}

int write_pipe(int fd, char * buf, int count) {
    int idx = get_pipe_from_fd(fd);
    if (idx < 0) {
        return -1; 
    }

    pipe_t * pipe = pipes[idx];

    if (fd == pipe->read_fd) { // no se puede escribir en el extremo de lectura
        return -1;
    }

    for (int i = 0; i < count; i++) {
        sem_wait(pipe->write_sem);
        pipe->buffer[pipe->write_idx] = buf[i];
        pipe->write_idx = (pipe->write_idx + 1) % PIPE_BUFFER_SIZE;
        sem_post(pipe->read_sem);
    }

    return count;
}



void destroy_pipe(int fd) {
    int idx = get_pipe_from_fd(fd);
    if (idx < 0) {
        return;
    }

    MemoryManagerADT mm = get_kernel_memory_manager();
    pipe_t * pipe = pipes[idx];

    sem_close(pipe->read_sem);
    sem_close(pipe->write_sem);
    free_memory(mm, pipe);
    pipes[idx] = NULL;
}