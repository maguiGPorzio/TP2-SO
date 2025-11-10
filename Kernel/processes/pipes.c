// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdbool.h>
#include "pipes.h"
#include "scheduler.h"
#include "lib.h"
#include "synchro.h"
#include "queue.h"
#include "videoDriver.h"

typedef struct pipe {
    char buffer[PIPE_BUFFER_SIZE]; // buffer circular
    char name[MAX_PIPE_NAME_LENGTH];
    int read_fd;
    int write_fd;
    int read_idx;
    int write_idx;
    int reader_count;
    int writer_count;
    char read_sem[SEM_NAME_SIZE];
    char write_sem[SEM_NAME_SIZE];
} pipe_t;


static pipe_t * pipes[MAX_PIPES] = {NULL};
static queue_t free_indexes = NULL;

static int get_free_idx() {
    return q_poll(free_indexes);
}

// devuelve el que seria el fd de read
static int get_fd_from_idx(int idx) {
    if (idx < 0 || idx >= MAX_PIPES) {
        return -1;
    }
    return (FIRST_FREE_FD + FIRST_FREE_FD % 2) + 2 * idx;
}


static int get_idx_from_fd(int fd) {
    
    int base = FIRST_FREE_FD + FIRST_FREE_FD % 2;  // primer fd par disponible
    int fd_par = fd - fd % 2;
    
    int idx = (fd_par - base) / 2;
    
    return (idx < 0 || idx >= MAX_PIPES) ? -1 : idx;
}

static pid_t pipe_find_process_by_fd(int fd, bool match_read_fd) {
    for (pid_t pid = 0; pid <= MAX_PID; pid++) {
        PCB *process = scheduler_get_process(pid);
        if (process == NULL || process->status == PS_TERMINATED) {
            continue;
        }

        if (match_read_fd) {
            if (process->read_fd == fd) {
                return pid;
            }
        } else if (process->write_fd == fd) {
            return pid;
        }
    }
    return NO_PID;
}

static bool pipe_process_is_alive(pid_t pid) {
    PCB *process = scheduler_get_process(pid);
    return process != NULL && process->status != PS_TERMINATED;
}

int init_pipes() {
    free_indexes = q_init();
    if (!free_indexes) {
        return 0;
    }

    for (int i = 0; i < MAX_PIPES; i++) {
        q_add(free_indexes, i);
    }

    return 1;
}

int create_pipe(int fds[2]) {
    int idx = get_free_idx();
    if (idx < 0) {
        return -1;
    }

    memory_manager_ADT mm = get_kernel_memory_manager();
    pipe_t * pipe = alloc_memory(mm, sizeof(pipe_t));
    if (pipe == NULL) {
        return -1;
    }

    pipes[idx] = pipe;

    pipe->read_idx = 0;
    pipe->write_idx = 0;
    pipe->reader_count = 1;
    pipe->writer_count = 1;
    pipe->name[0] = '\0';  // Pipe anónimo (sin nombre)
    
    
    pipe->read_fd = get_fd_from_idx(idx);      // fd par (lectura)
    pipe->write_fd = pipe->read_fd + 1;         // fd impar (escritura)
    fds[0] = pipe->read_fd;
    fds[1] = pipe->write_fd;

    // semaforo para leer
    decimal_to_str(pipe->read_fd, pipe->read_sem);
    strcat(pipe->read_sem, "r");
    if (sem_open(pipe->read_sem, 0) < 0) {
        pipes[idx] = NULL;
        free_memory(mm, pipe);
        return -1; 
    }
    
    // semaforo para escribir
    decimal_to_str(pipe->write_fd, pipe->write_sem);
    strcat(pipe->write_sem, "w");
    if (sem_open(pipe->write_sem, PIPE_BUFFER_SIZE) < 0) {
        sem_close(pipe->read_sem);
        pipes[idx] = NULL;
        free_memory(mm, pipe);
        return -1;
    }

    return idx;
}

// Busca un pipe por nombre, retorna su índice o -1 si no existe
static int find_pipe_by_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i] != NULL && strcmp(pipes[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int open_pipe(char *name, int fds[2]) {
    if (name == NULL || name[0] == '\0') {
        return -1;  // Nombre inválido
    }
    
    
    int idx = find_pipe_by_name(name);
    
    if (idx >= 0) {
        // ya existe
        pipe_t *pipe = pipes[idx];
        
        // si ya cerraron los writers no permitir reabrir
        // puede haber readers que hayan visto eof
        if (pipe->writer_count == 0) {
            return -1;
        }
        
        fds[0] = pipe->read_fd;
        fds[1] = pipe->write_fd;
        pipe->reader_count++;
        pipe->writer_count++;
        return idx;
    }
    
    // no existe -> crearlo
    idx = create_pipe(fds);
    if (idx < 0) {
        return -1;
    }
    
    pipe_t *pipe = pipes[idx];
    strncpy(pipe->name, name, MAX_PIPE_NAME_LENGTH - 1);
    pipe->name[MAX_PIPE_NAME_LENGTH - 1] = '\0';
    
    return idx;
}

// este solo va a ser llamado desde kernel cuando se pipean procesos
int open_fd(int fd) {
    int idx = get_idx_from_fd(fd);
    if (idx < 0) {
        return -1;
    }
    
    pipe_t *pipe = pipes[idx];
    if (pipe == NULL) {
        return -1;
    }

    if (fd == pipe->read_fd) {
        pipe->reader_count++;
        return 1;
    }

    if (fd == pipe->write_fd) {
        // Si ya no hay writers (llegó a 0), no permitir nuevos writers
        if (pipe->writer_count == 0) {
            return -1;  // No se pueden agregar writers después de que todos cerraron
        }
        pipe->writer_count++;
        return 1;
    }
    
    return 0;
}

int close_fd(int fd) {
    int idx = get_idx_from_fd(fd);
    if (idx < 0) {
        return -1;
    }
    
    pipe_t *pipe = pipes[idx];
    if (pipe == NULL) {
        return -1;
    }

    if (fd == pipe->read_fd) {
        if (pipe->reader_count > 0) {
            pipe->reader_count--;
        }
        
        // Si no quedan readers ni writers, destruir el pipe
        if (pipe->reader_count == 0 && pipe->writer_count == 0) {
            destroy_pipe(idx);
        }
        return 1;
    }

    if (fd == pipe->write_fd) {
        if (pipe->writer_count > 0) {
            pipe->writer_count--;
            
            // si era el ultimo writer, despertar readers potencialmente bloqueados para que vean eof
            if (pipe->writer_count == 0) {
                
                for (int i = 0; i < pipe->reader_count; i++) {
                    sem_post(pipe->read_sem);
                }
            }
        }
        
        // Si no quedan readers ni writers, destruir el pipe
        if (pipe->reader_count == 0 && pipe->writer_count == 0) {
            destroy_pipe(idx);
        }
        return 1;
    }
    
    return 0;
}

int read_pipe(int fd, char * buf, int count) {
    int idx = get_idx_from_fd(fd);
    if (idx < 0) {
        return -1;
    }

    pipe_t * pipe = pipes[idx];
    if (pipe == NULL) {
        return -1;
    }

    if (fd == pipe->write_fd) { // no se puede leer en el extremo de escritura
        return -1;
    }
    

    for (int i = 0; i < count; i++) {
        // verificar eof sin bloquear
        if (pipe->writer_count == 0 && pipe->read_idx == pipe->write_idx) {
            return i;
        }
        
        sem_wait(pipe->read_sem);
        
        // volvemos a chequear por las dudas de que haya cerrado mientras estabamos bloqueados
        if (pipe->writer_count == 0 && pipe->read_idx == pipe->write_idx) {
            sem_post(pipe->read_sem);
            return i;
        }
        
        buf[i] = pipe->buffer[pipe->read_idx];
        pipe->read_idx = (pipe->read_idx + 1) % PIPE_BUFFER_SIZE;
        sem_post(pipe->write_sem);
    }

    return count;
}

int write_pipe(int fd, const char * buf, int count) {
    int idx = get_idx_from_fd(fd);
    if (idx < 0) {
        return -1; 
    }

    pipe_t * pipe = pipes[idx];
    if (pipe == NULL) {
        return -1;
    }

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

void pipe_on_process_killed(pid_t victim) {
    PCB *victim_process = scheduler_get_process(victim);
    if (victim_process == NULL) {
        return;
    }

    int victim_read_fd = victim_process->read_fd;
    int victim_write_fd = victim_process->write_fd;

    for (int i = 0; i < MAX_PIPES; i++) {
        pipe_t *pipe = pipes[i];
        if (pipe == NULL) {
            continue;
        }

        bool victim_is_reader = (victim_read_fd == pipe->read_fd);
        bool victim_is_writer = (victim_write_fd == pipe->write_fd);

        if (!victim_is_reader && !victim_is_writer) {
            continue;
        }

        if (victim_is_reader) {
            pid_t writer_pid = pipe_find_process_by_fd(pipe->write_fd, false);
            if (writer_pid != NO_PID && writer_pid != victim && pipe_process_is_alive(writer_pid)) {
                scheduler_kill_process(writer_pid);
            }
        }

        if (victim_is_writer) {
            pid_t reader_pid = pipe_find_process_by_fd(pipe->read_fd, true);
            if (reader_pid != NO_PID && reader_pid != victim && pipe_process_is_alive(reader_pid)) {
                scheduler_kill_process(reader_pid);
            }
        }

        // Destruir el pipe después de matar al otro proceso
        destroy_pipe(i);
    }
}


void destroy_pipe(int idx) {
    if (idx < 0 || idx >= MAX_PIPES) {
        return;
    }

    pipe_t * pipe = pipes[idx];
    if (pipe == NULL) {
        return;
    }

    memory_manager_ADT mm = get_kernel_memory_manager();

    sem_close(pipe->read_sem);
    sem_close(pipe->write_sem);
    free_memory(mm, pipe);
    pipes[idx] = NULL;
    
    // Devolver el índice a la cola de libres
    q_add(free_indexes, idx);
}


int pipes_info(pipe_info_t * buf, int max_count) {
    if (buf == NULL || max_count <= 0) {
        return -1;
    }

    int count = 0;
    for (int i = 0; i < MAX_PIPES && count < max_count; i++) {
        pipe_t *pipe = pipes[i];
        if (pipe == NULL) {
            continue;
        }
        
        buf[count].id = i;
        strncpy(buf[count].name, pipe->name, MAX_PIPE_NAME_LENGTH);
        buf[count].read_fd = pipe->read_fd;
        buf[count].write_fd = pipe->write_fd;
        buf[count].readers = pipe->reader_count;
        buf[count].writers = pipe->writer_count;
        buf[count].buffered = (pipe->write_idx - pipe->read_idx + PIPE_BUFFER_SIZE) % PIPE_BUFFER_SIZE;
        
        count++;
    }

    return count;
}



