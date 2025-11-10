#ifndef PIPES_H
#define PIPES_H

#include <stdint.h>
#include "memory_manager.h"
#include "fds.h"
#include "scheduler.h"

#define PIPE_BUFFER_SIZE 1024
#define MAX_PIPES 32
#define MAX_PIPE_NAME_LENGTH 32
#define SEM_NAME_SIZE 32
#define EOF -1

typedef struct pipe_info {
    int id;
    char name[MAX_PIPE_NAME_LENGTH];
    int read_fd;
    int write_fd;
    int readers;
    int writers;
    int buffered;
} pipe_info_t;

int init_pipes();

// deja en fd[0] el read_fd y en fd[1] el write_fd
// devuelve el id del pipe, -1 si no lo creo
int create_pipe(int fds[2]);

// devuelve cuantos bytes leyo, -1 si falla
int read_pipe(int fd, char * buf, int count);

// devuelve cuantos bytes escribio, -1 si falla
int write_pipe(int fd, const char * buf, int count);

// libera los recursos del pipe
void destroy_pipe(int idx); 

// open para un pipe con nombre, si no existe lo crea y devuelve ambos fds
// retorna -1 si el pipe existe pero ya cerró todos sus writers (protección EOF)
int open_pipe(char * name, int fds[2]);

// agrega al pipe un reader / writer segun el fd
// retorna -1 si es write_fd y writer_count ya llegó a 0 (protección EOF)
int open_fd(int fd);

// sacar un reader / writer
// si writer_count llega a 0, hace posts para despertar readers bloqueados
// si ambos counts llegan a 0, destruye el pipe automáticamente
int close_fd(int fd);

// para matar el foreground gruop
void pipe_on_process_killed(pid_t victim);


// llena el buffer con información de los pipes activos
// retorna el número de pipes copiados, o -1 si hay error
int pipes_info(pipe_info_t * buf, int max_count);

#endif
