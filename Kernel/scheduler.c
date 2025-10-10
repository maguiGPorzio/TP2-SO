// Kernel/scheduler.c
#include "scheduler.h"
#include "memoryManager.h"
#include "naiveConsole.h"
#include <stddef.h>

// ============================================
//           CONFIGURACIÓN
// ============================================

#define MAX_PROCESSES 64
#define DEFAULT_QUANTUM 10
#define STACK_SIZE (4096 * 4)
#define IDLE_PID 0

// ============================================
//           ESTRUCTURAS INTERNAS
// ============================================

typedef struct {
    Process processes[MAX_PROCESSES];
    Process* ready_queue;
    Process* current_process;
    Process* idle_process;
    pid_t next_pid;
    uint64_t total_processes;
    bool initialized;
} Scheduler;

static Scheduler scheduler = {0};

// ============================================
//     FUNCIONES AUXILIARES DE STRING
// ============================================

static void copy_string(char* dest, const char* src) {
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

static void copy_string_n(char* dest, const char* src, size_t n) {
    size_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// ============================================
//           FUNCIONES AUXILIARES
// ============================================

static Process* find_free_process_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler.processes[i].state == KILLED) {
            return &scheduler.processes[i];
        }
    }
    return NULL;
}

static Process* find_process_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler.processes[i].pid == pid && 
            scheduler.processes[i].state != KILLED) {
            return &scheduler.processes[i];
        }
    }
    return NULL;
}

static void enqueue_ready(Process* process) {
    if (process == NULL) return;
    
    process->state = READY;
    process->next = NULL;
    process->prev = NULL;
    
    if (scheduler.ready_queue == NULL) {
        scheduler.ready_queue = process;
    } else {
        Process* current = scheduler.ready_queue;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = process;
        process->prev = current;
    }
}

static Process* dequeue_ready(void) {
    if (scheduler.ready_queue == NULL) {
        return scheduler.idle_process;
    }
    
    Process* process = scheduler.ready_queue;
    scheduler.ready_queue = process->next;
    
    if (scheduler.ready_queue != NULL) {
        scheduler.ready_queue->prev = NULL;
    }
    
    process->next = NULL;
    process->prev = NULL;
    
    return process;
}

static void remove_from_ready_queue(Process* process) {
    if (process == NULL || scheduler.ready_queue == NULL) return;
    
    if (scheduler.ready_queue == process) {
        scheduler.ready_queue = process->next;
        if (scheduler.ready_queue != NULL) {
            scheduler.ready_queue->prev = NULL;
        }
    } else {
        if (process->prev != NULL) {
            process->prev->next = process->next;
        }
        if (process->next != NULL) {
            process->next->prev = process->prev;
        }
    }
    
    process->next = NULL;
    process->prev = NULL;
}

static void process_wrapper(void (*entry_point)(void)) {
    entry_point();
    kill_process(get_current_pid());
    while(1) {
        __asm__ __volatile__("hlt");
    }
}

static void idle_process_function(void) {
    while(1) {
        __asm__ __volatile__("hlt");
    }
}

// ============================================
//      IMPLEMENTACIÓN DEL SCHEDULER
// ============================================

void scheduler_init(void) {
    if (scheduler.initialized) {
        return;
    }
    
    ncPrint("Initializing scheduler...\n");
    
    scheduler.ready_queue = NULL;
    scheduler.current_process = NULL;
    scheduler.next_pid = 1;
    scheduler.total_processes = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        scheduler.processes[i].state = KILLED;
        scheduler.processes[i].pid = -1;
    }
    
    // Crear proceso IDLE
    scheduler.idle_process = &scheduler.processes[0];
    scheduler.idle_process->pid = IDLE_PID;
    
    // Copiar nombre "idle" sin strcpy
    copy_string(scheduler.idle_process->name, "idle");
    
    scheduler.idle_process->state = READY;
    scheduler.idle_process->priority = PRIORITY_LOW;
    scheduler.idle_process->quantum = DEFAULT_QUANTUM;
    scheduler.idle_process->total_cpu_time = 0;
    scheduler.idle_process->parent_pid = -1;
    scheduler.idle_process->is_foreground = false;
    scheduler.idle_process->next = NULL;
    scheduler.idle_process->prev = NULL;
    
    // Allocar stack
    MemoryManagerADT mm = getKernelMemoryManager();
    scheduler.idle_process->stack_base = allocMemory(mm, STACK_SIZE);
    
    if (scheduler.idle_process->stack_base == NULL) {
        ncPrint("ERROR: Cannot allocate stack for IDLE\n");
        return;
    }
    
    scheduler.idle_process->stack_size = STACK_SIZE;
    scheduler.idle_process->rsp = (uint64_t)scheduler.idle_process->stack_base + STACK_SIZE;
    scheduler.idle_process->rbp = scheduler.idle_process->rsp;
    scheduler.idle_process->rip = (uint64_t)idle_process_function;
    
    scheduler.initialized = true;
    
    ncPrint("Scheduler initialized\n");
}

void scheduler_start(void) {
    if (!scheduler.initialized) {
        ncPrint("ERROR: Scheduler not initialized\n");
        return;
    }
    
    ncPrint("Starting scheduler...\n");
    
    scheduler.current_process = scheduler.idle_process;
    scheduler.current_process->state = RUNNING;
    
    ncPrint("Scheduler started\n");
}

pid_t create_process(void (*entry_point)(void), char* name, ProcessPriority priority, bool is_foreground) {
    if (!scheduler.initialized) {
        return -1;
    }
    
    Process* process = find_free_process_slot();
    if (process == NULL) {
        ncPrint("ERROR: No free process slots\n");
        return -1;
    }
    
    pid_t pid = scheduler.next_pid++;
    
    process->pid = pid;
    
    // Copiar nombre sin strncpy
    copy_string_n(process->name, name, 32);
    
    process->state = READY;
    process->priority = priority;
    process->quantum = DEFAULT_QUANTUM;
    process->total_cpu_time = 0;
    process->parent_pid = get_current_pid();
    process->is_foreground = is_foreground;
    process->next = NULL;
    process->prev = NULL;
    
    // Allocar stack
    MemoryManagerADT mm = getKernelMemoryManager();
    process->stack_base = allocMemory(mm, STACK_SIZE);
    
    if (process->stack_base == NULL) {
        ncPrint("ERROR: Cannot allocate stack for process\n");
        process->state = KILLED;
        return -1;
    }
    
    process->stack_size = STACK_SIZE;
    
    uint64_t* stack_top = (uint64_t*)((char*)process->stack_base + STACK_SIZE);
    stack_top--;
    *stack_top = (uint64_t)entry_point;
    
    process->rsp = (uint64_t)stack_top;
    process->rbp = process->rsp;
    process->rip = (uint64_t)process_wrapper;
    
    enqueue_ready(process);
    
    scheduler.total_processes++;
    
    ncPrint("Process created: PID=");
    ncPrintDec(pid);
    ncPrint(" name=");
    ncPrint(name);
    ncPrint("\n");
    
    return pid;
}

void kill_process(pid_t pid) {
    if (pid == IDLE_PID) {
        ncPrint("ERROR: Cannot kill IDLE process\n");
        return;
    }
    
    Process* process = find_process_by_pid(pid);
    if (process == NULL) {
        return;
    }
    
    ncPrint("Killing process PID=");
    ncPrintDec(pid);
    ncPrint("\n");
    
    if (process->state == READY) {
        remove_from_ready_queue(process);
    }
    
    if (process->stack_base != NULL) {
        MemoryManagerADT mm = getKernelMemoryManager();
        freeMemory(mm, process->stack_base);
        process->stack_base = NULL;
    }
    
    process->state = KILLED;
    process->pid = -1;
    
    if (process == scheduler.current_process) {
        schedule();
    }
}

void block_process(pid_t pid) {
    Process* process = find_process_by_pid(pid);
    if (process == NULL || process->state == KILLED) {
        return;
    }
    
    if (process->state == READY) {
        remove_from_ready_queue(process);
    }
    
    process->state = BLOCKED;
    
    if (process == scheduler.current_process) {
        schedule();
    }
}

void unblock_process(pid_t pid) {
    Process* process = find_process_by_pid(pid);
    if (process == NULL || process->state != BLOCKED) {
        return;
    }
    
    enqueue_ready(process);
}

void yield(void) {
    schedule();
}

Process* get_current_process(void) {
    return scheduler.current_process;
}

pid_t get_current_pid(void) {
    if (scheduler.current_process == NULL) {
        return -1;
    }
    return scheduler.current_process->pid;
}

void schedule(void) {
    if (!scheduler.initialized) {
        return;
    }
    
    Process* old_process = scheduler.current_process;
    
    if (old_process != NULL && 
        old_process->state == RUNNING && 
        old_process != scheduler.idle_process) {
        old_process->state = READY;
        enqueue_ready(old_process);
    }
    
    Process* new_process = dequeue_ready();
    
    if (new_process == NULL) {
        new_process = scheduler.idle_process;
    }
    
    new_process->state = RUNNING;
    new_process->quantum = DEFAULT_QUANTUM;
    
    scheduler.current_process = new_process;
}

void scheduler_tick(void) {
    if (!scheduler.initialized || scheduler.current_process == NULL) {
        return;
    }
    
    Process* current = scheduler.current_process;
    
    current->total_cpu_time++;
    
    if (current->quantum > 0) {
        current->quantum--;
    }
    
    if (current->quantum == 0) {
        schedule();
    }
}

void print_processes(void) {
    ncPrint("\n=== PROCESS LIST ===\n");
    ncPrint("PID\tNAME\t\tSTATE\t\tPRIO\tCPU\n");
    ncPrint("---\t----\t\t-----\t\t----\t---\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process* p = &scheduler.processes[i];
        if (p->state == KILLED) continue;
        
        ncPrintDec(p->pid);
        ncPrint("\t");
        ncPrint(p->name);
        ncPrint("\t\t");
        
        switch(p->state) {
            case READY:   ncPrint("READY");   break;
            case RUNNING: ncPrint("RUNNING"); break;
            case BLOCKED: ncPrint("BLOCKED"); break;
            case KILLED:  ncPrint("KILLED");  break;
        }
        ncPrint("\t\t");
        
        ncPrintDec(p->priority);
        ncPrint("\t");
        ncPrintDec(p->total_cpu_time);
        ncPrint("\n");
    }
    
    ncPrint("====================\n\n");
}