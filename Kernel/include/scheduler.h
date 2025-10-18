// Cooperative scheduler interface
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "processes.h"

// Entry point from int 0x80 handler: only patches the iret frame for cooperative switch
void scheduler_tick_from_syscall(uint64_t *syscall_frame_rsp, uint64_t syscall_id);

// Introspection helpers
int  scheduler_current_index(void);
PCB *scheduler_current_pcb(void);

#endif // SCHEDULER_H
