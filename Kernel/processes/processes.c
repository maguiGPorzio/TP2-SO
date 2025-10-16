// Minimal cooperative scheduler and processes implementation
#include <stdint.h>
#include <stddef.h>
#include <lib.h>
#include "memoryManager.h"
#include "processes.h"

// ========================= Internal State =========================
static PCB proc_table[MAX_PROCESSES];
static int pid_in_use[MAX_PROCESSES];
static int current_index = -1; // index into proc_table, -1 means no current

// Forward decls
static int alloc_pid(void);
static void free_pid(int pid);
static void prepare_initial_stack(PCB *p);
static void process_start_trampoline(void);

// ========================= API =========================
void init_processes(void) {
	for (int i = 0; i < MAX_PROCESSES; i++) {
		pid_in_use[i] = 0;
		proc_table[i].status = PS_TERMINATED;
		proc_table[i].pid = NO_PID;
	}
}

int proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name) {
	if (entry == NULL) return NO_PID;

	// Find free slot
	int slot = -1;
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (!pid_in_use[i]) { slot = i; break; }
	}
	if (slot < 0) return NO_PID;

	int pid = alloc_pid();
	if (pid == NO_PID) return NO_PID;

	PCB *p = &proc_table[slot];
	memset(p, 0, sizeof(*p));

	p->pid = pid;
	p->parent_pid = (current_index >= 0) ? proc_table[current_index].pid : NO_PID;
	p->status = PS_READY;
	p->entry = entry;
	p->argc = argc;

	// Duplicate argv in kernel heap: array of char* and each string
	if (argc > 0 && argv != NULL) {
		p->argv = (char**)allocMemory(getKernelMemoryManager(), sizeof(char*) * (size_t)argc);
		if (p->argv == NULL) { free_pid(pid); return NO_PID; }
		for (int i = 0; i < argc; i++) {
			size_t len = 0; while (argv[i] && ((const char*)argv[i])[len] != '\0') len++;
			char *copy = (char*)allocMemory(getKernelMemoryManager(), len + 1);
			if (copy == NULL) { p->argv = NULL; break; }
			for (size_t j = 0; j <= len; j++) copy[j] = ((const char*)argv[i])[j];
			p->argv[i] = copy;
		}
	} else {
		p->argv = NULL;
	}

	// Copy name
	if (name) {
		for (int i = 0; i < MAX_NAME_LENGTH - 1 && name[i]; i++) p->name[i] = name[i];
	}

	// Allocate stack
	p->stack_base = allocMemory(getKernelMemoryManager(), PROCESS_STACK_SIZE);
	if (!p->stack_base) { free_pid(pid); return NO_PID; }
	// Zero stack and set initial stack pointer to top (grows down)
	memset(p->stack_base, 0, PROCESS_STACK_SIZE);
	p->stack_pointer = (void*)((uint8_t*)p->stack_base + PROCESS_STACK_SIZE);

	// Prepare first context so that it runs processCaller(entry, argc, argv)
	prepare_initial_stack(p);

	// Mark slot occupied
	pid_in_use[slot] = 1;

	// Don't change current here; current will remain whoever is running until a yield/exit occurs

	return pid;
}

void proc_exit(int status) {
	if (current_index < 0) return;
	PCB *cur = &proc_table[current_index];
	cur->return_value = status;
	cur->status = PS_TERMINATED;
}

int proc_getpid(void) {
	if (current_index < 0) return NO_PID;
	return proc_table[current_index].pid;
}

void proc_yield(void) {
	// Nothing here: actual switch is done by scheduler_tick_from_syscall
}

int proc_waitpid(int pid) {
	// Minimal stub: not implemented yet
	(void)pid; return NO_PID;
}

// ========================= Scheduler =========================
// This function is called from syscall handler with a pointer to the saved
// interrupt/syscall frame on the current kernel stack (RSP at entry of handler).
// We can replace the user RIP/CS/RFLAGS/RSP/SS to jump into another process.
// Our processes start in userland flat mode at address of processCaller.

// Offsets into the stack at iret frame within our ISR wrapper in idt/interrupts.asm
// After popping our saved state, the stack layout is:
// [rsp+0] = RIP, [rsp+8] = CS, [rsp+16] = RFLAGS, [rsp+24] = RSP, [rsp+32] = SS
typedef struct {
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} iret_frame_t;

static void ensure_init_process(iret_frame_t *frame) {
	if (current_index >= 0) return;
	// Create a PCB representing the current running context (init)
	int slot = -1;
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (!pid_in_use[i]) { slot = i; break; }
	}
	if (slot < 0) return;
	int pid = alloc_pid();
	if (pid == NO_PID) return;
	PCB *p = &proc_table[slot];
	memset(p, 0, sizeof(*p));
	p->pid = pid;
	p->parent_pid = NO_PID;
	p->status = PS_RUNNING;
	p->stack_pointer = (void*)frame->rsp; // saved user RSP
	p->rip = frame->rip;                  // saved user RIP
	pid_in_use[slot] = 1;
	current_index = slot;
}

void scheduler_tick_from_syscall(uint64_t *syscall_frame_rsp, uint64_t syscall_id) {
	// Cooperative: pick next READY process; if current is RUNNING keep it unless it called yield/exit
	int start = (current_index >= 0) ? current_index : 0;

	iret_frame_t *frame = (iret_frame_t*)syscall_frame_rsp;
	ensure_init_process(frame);

	// If current is terminated, free its resources
	if (current_index >= 0 && proc_table[current_index].status == PS_TERMINATED) {
		PCB *cur = &proc_table[current_index];
		// free argv
		if (cur->argv) {
			for (int i = 0; i < cur->argc; i++) {
				if (cur->argv[i]) freeMemory(getKernelMemoryManager(), cur->argv[i]);
			}
			freeMemory(getKernelMemoryManager(), cur->argv);
			cur->argv = NULL;
		}
		// free stack
		if (cur->stack_base) freeMemory(getKernelMemoryManager(), cur->stack_base);
		free_pid(cur->pid);
		cur->pid = NO_PID;
		cur->status = PS_TERMINATED;
		pid_in_use[current_index] = 0;
		current_index = -1;
	}

	// Only consider switching on yield (29) or exit (27)
	int should_switch = (syscall_id == 29) || (syscall_id == 27);
	if (!should_switch) return;

	// Simple round-robin to find next READY
	int next = -1;
	for (int i = 1; i <= MAX_PROCESSES; i++) {
		int idx = (start + i) % MAX_PROCESSES;
		if (pid_in_use[idx] && proc_table[idx].status == PS_READY) { next = idx; break; }
		if (pid_in_use[idx] && current_index < 0 && proc_table[idx].status == PS_RUNNING) { next = idx; break; }
	}

	if (next < 0) {
		// If no READY, keep current if exists
		if (current_index >= 0) return;
		else return; // nothing to run
	}

	// Save current context: store user RSP and RIP from the iret frame
	if (current_index >= 0 && proc_table[current_index].status == PS_RUNNING) {
		proc_table[current_index].stack_pointer = (void*)frame->rsp;
		proc_table[current_index].rip = frame->rip;
		proc_table[current_index].status = PS_READY;
	}

	// Switch to next
	current_index = next;
	PCB *p = &proc_table[current_index];
	p->status = PS_RUNNING;

	// Patch iret frame to resume next process with its saved RSP. First time it points to prepared stack.
	frame->rsp = (uint64_t)p->stack_pointer;
	frame->rip = p->rip;
}

// ========================= Helpers =========================
static int alloc_pid(void) {
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (!pid_in_use[i]) {
			pid_in_use[i] = 1;
			return i; // PID == index for simplicity
		}
	}
	return NO_PID;
}

static void free_pid(int pid) {
	if (pid >= 0 && pid < MAX_PROCESSES) pid_in_use[pid] = 0;
}

// Layout we push on the new process stack so that when restored it executes:
//   processCaller(entry, argc, argv)
// We craft a "call" frame to return to nowhere (halt) if it returns.
extern void haltcpu(void);

static void prepare_initial_stack(PCB *p) {
	uint64_t *sp = (uint64_t*)p->stack_pointer;

	// Align and build a call frame: push a fake return address (haltcpu)
	*(--sp) = (uint64_t)haltcpu; // return address for processCaller

	// Set up arguments (System V AMD64 ABI): rdi=entry, rsi=argc, rdx=argv
	// We'll simulate a call by setting up a small trampoline frame where the first RET goes into processCaller
	// To pass regs, we rely on that when the process first gets CPU, it will resume as if it had just executed a call to processCaller.

	// Reserve space for callee-saved registers if needed (not strictly necessary here)

	// Push argument shadow space (not required in SysV, but keep stack 16-byte aligned)
	if (((uint64_t)sp & 0xF) != 0) { --sp; }

	// Store initial register values on stack frame that our int handler will iret to.
	// Our ISR will iret to RIP that continues after int; but we want the first time to jump into a small shim
	// Simpler approach: set the process stack so that the first userland RET jumps to processCaller with the right register values

	// Save stack pointer back
	p->stack_pointer = sp;

	// Set initial entry point for first schedule to our trampoline
	p->rip = (uint64_t)process_start_trampoline;
}

// C-level caller for a process: called with proper arguments, never returns
static PCB * current_pcb(void) { return (current_index >= 0) ? &proc_table[current_index] : NULL; }

static void process_start_trampoline(void) {
	PCB *p = current_pcb();
	int ret = 0;
	if (p && p->entry) {
		ret = p->entry(p->argc, p->argv);
	}
	proc_exit(ret);
	// Should not reach here
	while (1) {}
}

