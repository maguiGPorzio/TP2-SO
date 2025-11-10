// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <process.h>
#include <memoryManager.h>
#include "lib.h"
#include "scheduler.h"
#include "interrupts.h"
#include "pipes.h"


extern void *setup_initial_stack(void *caller, int pid, void *stack_pointer, void *rcx);
static char **duplicate_argv(const char **argv, int argc, memory_manager_ADT mm);
static void process_caller(int pid);
static void init_pcb_base_fields(PCB *p, int pid, process_entry_t entry, const char *name, bool killable);
static int init_pcb_stack(PCB *p, memory_manager_ADT mm);
static int init_pcb_argv(PCB *p, int argc, const char **argv, memory_manager_ADT mm);
static void init_pcb_file_descriptors(PCB *p, int fds[2]);
static void free_pcb_argv(PCB *p, memory_manager_ADT mm);
static void free_pcb_stack(PCB *p, memory_manager_ADT mm);

static void init_pcb_base_fields(PCB *p, int pid, process_entry_t entry, const char *name, bool killable) {
    p->pid = pid;
    p->parent_pid = scheduler_get_current_pid();
    strncpy(p->name, name, MAX_PROCESS_NAME_LENGTH - 1);
    p->name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
    p->entry = entry;
    p->return_value = 0;
    p->waiting_on = NO_PID;
    p->killable = killable;
}

static int init_pcb_stack(PCB *p, memory_manager_ADT mm) {
    p->stack_base = alloc_memory(mm, PROCESS_STACK_SIZE);
    if (p->stack_base == NULL) {
        return ERROR;
    }
    p->stack_pointer = setup_initial_stack(&process_caller, p->pid, (char *)p->stack_base + PROCESS_STACK_SIZE, 0);
    return OK;
}


static int init_pcb_argv(PCB *p, int argc, const char **argv, memory_manager_ADT mm) {
    p->argc = argc;
    if (argc > 0 && argv != NULL) {
        p->argv = duplicate_argv(argv, argc, mm);
        if (p->argv == NULL) {
            return ERROR;
        }
    } else {
        p->argv = NULL;
    }
    return OK;
}

static void init_pcb_file_descriptors(PCB *p, int fds[2]) {
    p->open_fds = q_init();
    
    if (fds == NULL) {
        p->read_fd = STDIN;
        p->write_fd = STDOUT;
    } else {
        p->read_fd = fds[0];
        if (fds[0] >= FIRST_FREE_FD) {
            open_fd(fds[0]);
            q_add(p->open_fds, fds[0]);
        }
        p->write_fd = fds[1];
        if (fds[1] >= FIRST_FREE_FD) {
            open_fd(fds[1]);
            q_add(p->open_fds, fds[1]);
        }
    }
}


PCB* proc_create(int pid, process_entry_t entry, int argc, const char **argv, const char *name, bool killable, int fds[2]) {

    if (!entry || !name || argc < 0) {
        return NULL;
    }

    memory_manager_ADT mm = get_kernel_memory_manager();

    PCB *p = alloc_memory(mm, sizeof(PCB));
    if (!p) {
        return NULL;
    }

    init_pcb_base_fields(p, pid, entry, name, killable);

    if (init_pcb_stack(p, mm) == ERROR) {
        free_memory(mm, p);
        return NULL;
    }

    if (init_pcb_argv(p, argc, argv, mm) == ERROR) {
        free_memory(mm, p->stack_base);
        free_memory(mm, p);
        return NULL;
    }

    init_pcb_file_descriptors(p, fds);

    return p;
}


static void free_pcb_argv(PCB *p, memory_manager_ADT mm) {
    if (p->argv != NULL) {
        for (int i = 0; i < p->argc; i++) {
            if (p->argv[i] != NULL) {
                free_memory(mm, p->argv[i]);
            }
        }
        free_memory(mm, p->argv);
        p->argv = NULL;
    }
}

static void free_pcb_stack(PCB *p, memory_manager_ADT mm) {
    if (p->stack_base != NULL) {
        free_memory(mm, p->stack_base);
        p->stack_base = NULL;
        p->stack_pointer = NULL;
    }
}


void free_process_resources(PCB * p) {
    if (p == NULL) {
        return;
    }

    memory_manager_ADT mm = get_kernel_memory_manager();

    free_pcb_argv(p, mm);
    free_pcb_stack(p, mm);

    q_destroy(p->open_fds);
    p->open_fds = NULL;

    // Liberar PCB
    free_memory(mm, p);
}


static char **duplicate_argv(const char **argv, int argc, memory_manager_ADT mm) {
    // Caso argv vacío o NULL: crear argv minimal con solo NULL
    if (argv == NULL || argc == 0 || (argc > 0 && argv[0] == NULL)) {
        char **new_argv = alloc_memory(mm, sizeof(char *));
        if (new_argv == NULL) {
            return NULL;
        }
        new_argv[0] = NULL;
        return new_argv;
    }

    // Alocar array de punteros (argc + 1 para el NULL terminador)
    char **new_argv = alloc_memory(mm, (argc + 1) * sizeof(char *));
    if (new_argv == NULL) {
        return NULL;
    }

    // Copiar cada string
    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            new_argv[i] = NULL;
            continue;
        }

        size_t len = strlen(argv[i]) + 1;
        new_argv[i] = alloc_memory(mm, len);

        if (new_argv[i] == NULL) {
            // Error: liberar todo lo alocado hasta ahora (ROLLBACK)
            for (int j = 0; j < i; j++) {
                if (new_argv[j] != NULL) {
                    free_memory(mm, new_argv[j]);
                }
            }
            free_memory(mm, new_argv);
            return NULL;
        }

        memcpy(new_argv[i], argv[i], len);
    }

    new_argv[argc] = NULL;
    return new_argv;
}

//check
static void process_caller(int pid) {
    PCB *p = scheduler_get_process(pid);
    if (p == NULL || p->entry == NULL) {
        scheduler_exit_process(-1);
        return;
    }

    // Llamar a la función de entrada del proceso
    int res = p->entry(p->argc, p->argv);

    // Cuando retorna, terminar el proceso
    scheduler_exit_process(res);
}