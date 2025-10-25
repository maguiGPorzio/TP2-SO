#include <process.h>
#include <memoryManager.h>
#include "lib.h"
#include "scheduler.h"
#include "interrupts.h"

// Funciones ASM para context switching
extern void *setup_initial_stack(void *caller, int pid, void *stack_pointer, void *rcx);

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================
static char **duplicateArgv(const char **argv, int argc, MemoryManagerADT mm);
static void process_caller(int pid);

// ============================================
//         LIFECYCLE DE PROCESOS
// ============================================
PCB* proc_create(int pid, process_entry_t entry, int argc, const char **argv,
                const char *name) {

    if (entry == NULL || name == NULL) {
        return NULL;
    }

    MemoryManagerADT mm = getKernelMemoryManager();

    // Alocar PCB
    PCB *p = allocMemory(mm, sizeof(PCB));
    if (p == NULL) {
        return NULL;
    }

    // ============================
    // Inicialización de campos base
    // ============================
    p->pid = pid;
    p->parent_pid = scheduler_get_current_pid();
    p->status = PS_READY;
    p->entry = entry;
    p->return_value = 0; //FIJARSE COMO INCIIALIZARLO
    p->waiting_on = -1;

    // ============================
    // Pila del proceso
    // ============================
    p->stack_base = allocMemory(mm, PROCESS_STACK_SIZE);
    if (p->stack_base == NULL) {
        freeMemory(mm, p);
        return NULL;
    }

    // El stack crece hacia abajo, por lo tanto el puntero inicial está al final del bloque
    p->stack_pointer = setup_initial_stack(&process_caller, p->pid,
                                        (char *)p->stack_base + PROCESS_STACK_SIZE,
                                        0 );

    // ============================
    // Argumentos (argv / argc)
    // ============================
    p->argc = argc;
    if (argc > 0 && argv != NULL) {
        p->argv = duplicateArgv(argv, argc, mm);
        if (p->argv == NULL) {
            freeMemory(mm, p->stack_base);
            freeMemory(mm, p);
            return NULL;
        }
    } else {
        p->argv = NULL;
    }

    // ============================
    // Nombre del proceso
    // ============================
    strncpy(p->name, name, MAX_NAME_LENGTH - 1);
    p->name[MAX_NAME_LENGTH - 1] = '\0';

    return p;
}

void free_process_resources(PCB * p) {
    if (p == NULL) {
        return;
    }

    MemoryManagerADT mm = getKernelMemoryManager();

    // Liberar argv y sus strings
    if (p->argv != NULL) {
        for (int i = 0; i < p->argc; i++) {
            if (p->argv[i] != NULL) {
                freeMemory(mm, p->argv[i]);
            }
        }
        freeMemory(mm, p->argv);
        p->argv = NULL;
    }

    // Liberar stack
    if (p->stack_base != NULL) {
        freeMemory(mm, p->stack_base);
        p->stack_base = NULL;
        p->stack_pointer = NULL;
    }

    // Liberar PCB
    freeMemory(mm, p);
}

// ============================================
//        FUNCIONES AUXILIARES PRIVADAS
// ============================================

static char **duplicateArgv(const char **argv, int argc, MemoryManagerADT mm) {
    // Caso argv vacío o NULL: crear argv minimal con solo NULL
    if (argv == NULL || argc == 0 || (argc > 0 && argv[0] == NULL)) {
        char **new_argv = allocMemory(mm, sizeof(char *));
        if (new_argv == NULL) {
            return NULL;
        }
        new_argv[0] = NULL;
        return new_argv;
    }

    // Alocar array de punteros (argc + 1 para el NULL terminador)
    char **new_argv = allocMemory(mm, (argc + 1) * sizeof(char *));
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
        new_argv[i] = allocMemory(mm, len);

        if (new_argv[i] == NULL) {
            // Error: liberar todo lo alocado hasta ahora (ROLLBACK)
            for (int j = 0; j < i; j++) {
                if (new_argv[j] != NULL) {
                    freeMemory(mm, new_argv[j]);
                }
            }
            freeMemory(mm, new_argv);
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