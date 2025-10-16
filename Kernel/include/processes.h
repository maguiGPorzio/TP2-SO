// Simple processes API and PCB definition
#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES      32
#define MAX_NAME_LENGTH    32
#define NO_PID             (-1)
#define INIT_PID           (0)
#define PROCESS_STACK_SIZE (8192) // 8 KiB

typedef enum {
	PS_READY = 0,
	PS_BLOCKED,
	PS_RUNNING,
	PS_TERMINATED
} process_status_t;

typedef int (*process_entry_t)(int argc, char **argv);

typedef struct PCB {
	int                 pid;
	int                 parent_pid;
	process_status_t    status;
	uint8_t             started;       // 0 if never scheduled, 1 if already started

	// Stack management
	void               *stack_base;    // base (lowest address) of allocated stack
	void               *stack_pointer; // saved RSP for context switch
	uint64_t            rip;           // saved RIP for context switch

	// Program info
	process_entry_t     entry;
	char              **argv;
	int                 argc;
	char                name[MAX_NAME_LENGTH];

	// Exit / wait
	int                 return_value;
	int                 waiting_for_pid; // -1 if none (for optional waitpid)
} PCB;

// Lifecycle
int  proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name);
void proc_exit(int status);
int  proc_getpid(void);
void proc_yield(void);
int  proc_waitpid(int pid); // optional, returns exit status or NO_PID if not implemented

// Internal helpers (kernel-only)
void init_processes(void);
void scheduler_tick_from_syscall(uint64_t *syscall_frame_rsp, uint64_t syscall_id);

#endif // PROCESSES_H
