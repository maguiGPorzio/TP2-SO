#ifndef PIPES_H
#define PIPES_H

#include <stdint.h>
#include "memoryManager.h"
#include "fds.h"
#include "scheduler.h"

#define PIPE_BUFFER_SIZE 1024
#define MAX_PIPES 32
#define SEM_NAME_SIZE 32
#define EOF -1

int init_pipes();

// deja en fd[0] el read_fd y en fd[1] el write_fd
// devuelve el id del pipe, -1 si no lo creo
int create_pipe(int fds[2]);

// devuelve cuantos bytes leyo, -1 si falla
int read_pipe(int fd, char * buf, int count);

// devuelve cuantos bytes escribio, -1 si falla
int write_pipe(int fd, char * buf, int count);

// libera los recursos del pipe
void destroy_pipe(int idx); 

void pipe_on_process_killed(pid_t victim);

#endif
