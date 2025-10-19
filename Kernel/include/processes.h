// Simple processes API and PCB definition
#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES      32
#define MAX_NAME_LENGTH    32
#define NO_PID             (-1)
#define INIT_PID           (0)
#define PROCESS_STACK_SIZE (4096) // 4 KB (en el foro dijeron que esta bien)

typedef enum {
	PS_READY = 0,
	PS_BLOCKED,
	PS_RUNNING,
	PS_TERMINATED
} process_status_t;

typedef int (*process_entry_t)(int argc, char **argv);


typedef struct PCB {
	// info
	int pid;
	process_status_t status;

	// stack
	void *stack_base;    
	void *stack_pointer; 

	process_entry_t entry;
	char **argv;
	int argc;
	char name[MAX_NAME_LENGTH];

	int return_value;
} PCB;

void init_processes(); // este tiene que crear el proceso de la shell que va a ser el unico en principio

// Lifecycle
int  proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name);
void proc_exit(int status);
int  proc_getpid(void);
void proc_yield(void);

void proc_print();



#endif // PROCESSES_H
