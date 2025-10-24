#include <processes.h>
#include <memoryManager.h>
#include "lib.h"
#include "scheduler.h"
#include "interrupts.h"

// ============================================
//           VARIABLES GLOBALES
// ============================================

static PCB *processes[MAX_PROCESSES] = {0};
static int current_process = -1;

// Shell usa memoria estática (no heap) para evitar leaks
static PCB shell_pcb;
static char *shell_argv_static[1] = {NULL};

// Funciones ASM para context switching
extern void *setup_initial_stack(void *caller, int pid, void *stack_pointer);
extern void switch_to_rsp_and_iret(void *next_rsp);

// Variables compartidas con ASM para solicitar cambio de contexto
// ASM escribe current_kernel_rsp al entrar a la syscall (después de pushState)
// C escribe switch_to_rsp cuando quiere que el handler cambie a otra pila antes de iretq
void *current_kernel_rsp = 0;
void *switch_to_rsp = 0;
extern void *syscall_frame_ptr; // RSP de la shell capturado antes de cambiar a un proceso

// ============================================
//        DECLARACIONES AUXILIARES
// ============================================

static int assign_pid(void);
static char **duplicateArgv(const char **argv, int argc, MemoryManagerADT mm);
static void run_next_process(void);
static void process_caller(int pid);
static int pick_next_ready_from(int startIdx);
static void free_process_resources(PCB *p);
static void cleanup_terminated_processes(void);

// ============================================
//           INICIALIZACIÓN
// ============================================

void init_processes_for_test(void) {
    // Limpiar tabla de procesos (para tests sin shell)
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i] = NULL;
    }
    current_process = -1;
    current_kernel_rsp = 0;
    switch_to_rsp = 0;
}

void init_processes(void) {
    // Limpiar tabla de procesos
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i] = NULL;
    }
    current_process = -1;

    // Usar PCB estático para la shell (NO malloc para evitar leaks)
    PCB *p = &shell_pcb;

    p->pid = 0;
    p->status = PS_RUNNING;
    p->stack_base = NULL;      // Shell usa el stack del kernel
    p->stack_pointer = NULL;
    p->argc = 0;
    p->entry = NULL;
    p->argv = shell_argv_static;  // Array estático (no heap)
    p->return_value = 0;

    // Copiar nombre con null-termination garantizada
    strncpy(p->name, "shell", MAX_NAME_LENGTH - 1);
    p->name[MAX_NAME_LENGTH - 1] = '\0';

    // Registrar shell en la tabla
    processes[p->pid] = p;
    current_process = p->pid;

    // Ejecutar la shell desde el módulo cargado en 0x400000
    int (*shell_entry)(int, char **) = (int (*)(int, char **))0x400000;
    int ret = shell_entry(p->argc, p->argv);

    // Si la shell retorna (no debería en uso normal), terminarla
    proc_exit(ret);
}

// ============================================
//         LIFECYCLE DE PROCESOS
// ============================================
int proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name) {
    if (entry == NULL || name == NULL) {
        return -1;
    }

    // Limpiar procesos terminados ANTES de crear uno nuevo
    cleanup_terminated_processes();

    MemoryManagerADT mm = getKernelMemoryManager();

    // Asignar PID
    int pid = assign_pid();
    if (pid < 0) {
        return -1;  // No hay slots disponibles
    }

    // Alocar PCB
    PCB *p = allocMemory(mm, sizeof(PCB));
    if (p == NULL) {
        return -1;
    }

    p->pid = pid;
    p->status = PS_READY;
    p->entry = entry;
    p->return_value = 0;

    // Alocar stack para el proceso (4 KB)
    p->stack_base = allocMemory(mm, PROCESS_STACK_SIZE);
    if (p->stack_base == NULL) {
        freeMemory(mm, p);
        return -1;
    }
    p->stack_pointer = (char *)p->stack_base + PROCESS_STACK_SIZE;

    // Duplicar argumentos
    p->argv = duplicateArgv(argv, argc, mm);
    if (p->argv == NULL) {
        freeMemory(mm, p->stack_base);
        freeMemory(mm, p);
        return -1;
    }
    p->argc = argc;

    // Copiar nombre del proceso con null-termination garantizada
    strncpy(p->name, name, MAX_NAME_LENGTH - 1);
    p->name[MAX_NAME_LENGTH - 1] = '\0';

    // ===== INTEGRACIÓN CON SCHEDULER =====
    
    // Inicializar campos de scheduling
    p->priority = DEFAULT_PRIORITY;          // Prioridad por defecto (5)
    p->remaining_quantum = DEFAULT_PRIORITY + 1;  // Quantum inicial
    p->cpu_ticks = 0;                        // Sin ticks de CPU aún

    // Registrar en tabla de procesos
    processes[pid] = p;

    // Preparar stack inicial para la primera ejecución
    p->stack_pointer = setup_initial_stack(&process_caller, p->pid, p->stack_pointer);

    // Agregar proceso al scheduler
    SchedulerADT sched = get_scheduler();
    if (sched != NULL) {
        if (scheduler_add_process(p) < 0) {
            // Error al agregar al scheduler - rollback completo
            freeMemory(mm, p->argv);
            freeMemory(mm, p->stack_base);
            freeMemory(mm, p);
            processes[pid] = NULL;
            return -1;
        }
    }

    // Ya NO necesitamos marcar el actual como READY manualmente
    // El scheduler preemptivo se encarga de esto automáticamente
    
    return pid;
}

// void proc_exit(int status) {
//     if (current_process < 0) {
//         return;
//     }

//     PCB *p = processes[current_process];
//     if (p == NULL) {
//         return;
//     }

//     // Guardar valor de retorno
//     p->return_value = status;

//     // Marcar como terminado
//     p->status = PS_TERMINATED;

//     // ===== INTEGRACIÓN CON SCHEDULER =====
    
//     // Remover del scheduler
//     SchedulerADT sched = get_scheduler();
//     if (sched != NULL) {
//         scheduler_remove_process(p->pid);
//     }

//     // Forzar cambio de contexto inmediato
//     scheduler_force_reschedule();
    
//     // El timer interrupt hará el context switch
//     // En un sistema preemptivo, esperamos a la próxima interrupción
//     // Para forzarlo inmediatamente, hacemos un yield
//     void *next_rsp = switch_to_rsp;
//     switch_to_rsp = 0;

//     if (next_rsp != 0) {
//         switch_to_rsp_and_iret(next_rsp);
//     }

//     // Si llegamos aquí, loop infinito (no debería pasar)
//     while (1) {
//         __asm__ volatile("hlt");
//     }
// }

void proc_exit(int status) {
    int pid = scheduler_get_current_pid();
    if (pid < 0) {
        return;
    }

    PCB *p = processes[pid];
    if (p == NULL) {
        return;
    }

    // Guardar valor de retorno
    p->return_value = status;

    // Marcar como terminado
    p->status = PS_TERMINATED;

    // Remover del scheduler
    SchedulerADT sched = get_scheduler();
    if (sched != NULL) {
        scheduler_remove_process(p->pid);
    }

    // ===== LLAMAR AL SCHEDULER DIRECTAMENTE =====
    
    // Forzar reschedule
    scheduler_force_reschedule();
    
    // Llamar al scheduler con el frame de la shell (capturado por ASM)
    void *prev_rsp = syscall_frame_ptr ? syscall_frame_ptr : current_kernel_rsp;
    void *next_rsp = schedule(prev_rsp);
    
    // Pedir el cambio de contexto al handler de syscalls (más robusto)
    switch_to_rsp = next_rsp;
    if (next_rsp != 0) {
        __asm__ volatile (
            "mov $31, %%rax\n\t"
            "int $0x80\n\t"
            :
            :
            : "rax", "memory"
        );
    }

    // Si llegamos aquí, loop infinito
    while (1) {
        __asm__ volatile("hlt");
    }
}

int proc_getpid(void) {
    return scheduler_get_current_pid();
}

// void proc_yield(void) {
//     // ===== INTEGRACIÓN CON SCHEDULER =====
    
//     // Forzar reschedule
//     scheduler_force_reschedule();
    
//     // Trigger timer interrupt manualmente (o simplemente hacer syscall)
//     // La forma más simple es dejar que el siguiente timer tick lo maneje
    
//     // Alternativa: Forzar context switch inmediato
//     // (requiere llamar a schedule() desde aquí)
// }

void proc_yield(void) {
    // Limpiar procesos terminados
    cleanup_terminated_processes();
    
    // ===== LLAMAR AL SCHEDULER DIRECTAMENTE =====
    
    // Guardar RSP actual
    void *prev_rsp = current_kernel_rsp;
    
    // Llamar al scheduler (retorna nuevo RSP)
    void *next_rsp = schedule(prev_rsp);
    
    // Si cambió de proceso, hacer context switch
    if (next_rsp != prev_rsp && next_rsp != NULL) {
        switch_to_rsp = next_rsp;
    }
}

int proc_block(int pid) {
    // Si pid es -1, bloquear el proceso actual
    if (pid == -1) {
        pid = scheduler_get_current_pid();
    }
    
    // Validar PID usando macro
    if (IS_INVALID_PID(pid)) {
        return -1;
    }
    
    PCB *p = processes[pid];
    if (p == NULL) {
        return -1;  // Proceso no existe
    }
    
    // No se puede bloquear un proceso ya terminado
    if (p->status == PS_TERMINATED) {
        return -1;
    }
    
    // Cambiar estado a BLOCKED
    p->status = PS_BLOCKED;

    
    // ---
    // TODO: ver que hacer con el scheduler. Debe ser algo así: 
    scheduler_remove_process(p->pid);
    
    // Si estamos bloqueando el proceso actual, hacer context switch inmediato 
    if (pid == scheduler_get_current_pid()) {
        // Forzar reschedule
        scheduler_force_reschedule();
    }
    // ---

    return 0;  // Éxito
}

int proc_unblock(int pid) {
    // Validar PID usando macro
    if (IS_INVALID_PID(pid)) {
        return -1;
    }
    
    PCB *p = processes[pid];
    if (p == NULL) {
        return -1;  // Proceso no existe
    }
    
    // Cambiar estado a READY
    p->status = PS_READY;

    // No se puede desbloquear un proceso terminado
    if (p->status == PS_TERMINATED) {
        return -1;
    }
    
    // ---
    // TODO: ver que hacer con el scheduler. Debe ser algo así: 
    scheduler_add_process(p->pid);
    //---
    return 0;  // Éxito
}

void proc_print(void) {
    uint32_t color = 0xFFFFFF;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB *p = processes[i];
        if (p == NULL) {
            continue;
        }

        // Convertir PID a string
        char pid_buf[12];
        int n = p->pid;
        int k = 0;

        if (n == 0) {
            pid_buf[k++] = '0';
        } else {
            char tmp[12];
            int t = 0;
            while (n > 0 && t < (int)sizeof(tmp)) {
                tmp[t++] = (char)('0' + (n % 10));
                n /= 10;
            }
            // Invertir dígitos
            while (t > 0) {
                pid_buf[k++] = tmp[--t];
            }
        }
        pid_buf[k] = '\0';

        // Imprimir información del proceso
        vdPrint("PID: ", color);
        vdPrint(pid_buf, color);
        vdPrint(" | Name: ", color);
        vdPrint(p->name, color);
        vdPrint(" | State: ", color);

        switch (p->status) {
        case PS_RUNNING:
            vdPrint("RUNNING", color);
            break;
        case PS_READY:
            vdPrint("READY", color);
            break;
        case PS_BLOCKED:
            vdPrint("BLOCKED", color);
            break;
        case PS_TERMINATED:
            vdPrint("TERMINATED", color);
            break;
        default:
            vdPrint("UNKNOWN", color);
            break;
        }

        vdPrint("\n", color);
    }
}

void cleanup_all_processes(void) {
    MemoryManagerADT mm = getKernelMemoryManager();

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] != NULL) {
            // NO liberar shell (es estática)
            if (processes[i] == &shell_pcb) {
                processes[i] = NULL;
                continue;
            }

            // Liberar procesos normales
            if (processes[i]->argv != NULL) {
                for (int j = 0; j < processes[i]->argc; j++) {
                    if (processes[i]->argv[j] != NULL) {
                        freeMemory(mm, processes[i]->argv[j]);
                    }
                }
                freeMemory(mm, processes[i]->argv);
            }

            if (processes[i]->stack_base != NULL) {
                freeMemory(mm, processes[i]->stack_base);
            }

            freeMemory(mm, processes[i]);
            processes[i] = NULL;
        }
    }

    current_process = -1;
    current_kernel_rsp = 0;
    switch_to_rsp = 0;
}

// ============================================
//        FUNCIONES AUXILIARES PRIVADAS
// ============================================

static int assign_pid(void) {
    // Buscar primer slot NULL (nunca usado o ya limpiado)
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] == NULL) {
            return i;
        }
    }

    // No hay slots libres
    return -1;
}

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

static void process_caller(int pid) {
    PCB *p = processes[pid];
    if (p == NULL || p->entry == NULL) {
        proc_exit(-1);
        return;
    }

    current_process = pid; // mantener variable informativa

    // Llamar a la función de entrada del proceso
    int res = p->entry(p->argc, p->argv);

    // Cuando retorna, terminar el proceso
    proc_exit(res);
}

static int pick_next_ready_from(int startIdx) {
    if (startIdx < 0) {
        startIdx = 0;
    }

    // Buscar siguiente proceso READY de forma circular
    for (int k = 0; k < MAX_PROCESSES; k++) {
        int idx = (startIdx + k) % MAX_PROCESSES;
        if (processes[idx] != NULL && processes[idx]->status == PS_READY) {
            return idx;
        }
    }

    return -1;  // No hay procesos READY
}

static void free_process_resources(PCB *p) {
    if (p == NULL) {
        return;
    }

    // NO liberar shell estática
    if (p == &shell_pcb) {
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

static void cleanup_terminated_processes(void) {
    int running = scheduler_get_current_pid();
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (i == running) {
            continue;
        }
        if (processes[i] != NULL && processes[i]->status == PS_TERMINATED) {
            free_process_resources(processes[i]);
            processes[i] = NULL;
        }
    }
}

