#include <processes.h>
#include <memoryManager.h>
#include "lib.h"

// TODO: despues pasar todo lo de scheduling a otro archivo scheduler.c y en lo posible hacerlo adt
// onda la idea seria que si tiene estado interno sea ADT y sino que sea tipo libreria de funciones 
// processes tiene sentido que sea una libreria de funciones y scheduler un ADT

static PCB * processes[MAX_PROCESSES] = {0};
static int current_process = -1;
// lo manejo como un coso circular non preemptive

extern void * setup_initial_stack(void * caller, int pid, void * stack_pointer);
extern void switch_to_rsp_and_iret(void * next_rsp);

// Variables compartidas con ASM para solicitar un cambio de contexto
// ASM escribe current_kernel_rsp al entrar a la syscall (después de pushState)
// C escribe switch_to_rsp cuando quiere que el handler cambie a otra pila antes de iretq
void * current_kernel_rsp = 0;
void * switch_to_rsp = 0;


// auxiliares
static int asign_pid();
static char **duplicateArgv(const char **argv, int argc, MemoryManagerADT mm);
static void run_next_process();
static void process_caller(int pid);
static int pick_next_ready_from(int startIdx);
static void free_process_resources(PCB *p);



void init_processes(); // este tiene que crear el proceso de la shell que va a ser el unico en principio

void init_processes() { // TODO: limpiar y que use process_spawn
	// Limpiar tabla y estado
	for (int i = 0; i < MAX_PROCESSES; i++) {
		processes[i] = NULL; // TODO: al pedo
	}
	current_process = -1;

	MemoryManagerADT mm = getKernelMemoryManager();

	// Crear PCB para la shell (PID 0)
	PCB *p = allocMemory(mm, sizeof(PCB));
	if (p == NULL) {
		return; // sin memoria, no hay mucho más que podamos hacer aquí
	}
	p->pid = 0;
	p->status = PS_RUNNING;
	p->stack_base = NULL;
	p->stack_pointer = NULL;
	p->argc = 0;
	p->argv = duplicateArgv(NULL, 0, mm); // argv minimal
	if (p->argv == NULL) {
		freeMemory(mm, p);
		return;
	}
	strncpy(p->name, "shell", MAX_NAME_LENGTH);
	processes[p->pid] = p;
	current_process = p->pid;

	// Ejecutar la shell: entrypoint del módulo de código en 0x400000
	int (*shell_entry)(int, char**) = (int (*)(int, char**))0x400000;
	int ret = shell_entry(p->argc, p->argv);
	(void)ret; // si alguna vez retorna, podríamos hacer proc_exit(ret)
}

// Lifecycle
// siempre que spawnea un proceso es el proximo en correr
int proc_spawn(process_entry_t entry, int argc, const char **argv, const char *name) {
	MemoryManagerADT mm = getKernelMemoryManager();

	PCB * p = allocMemory(mm, sizeof(PCB));

	if (p == NULL) {
		return -1;
	}

	p->pid = asign_pid();
	if (p->pid < 0) {
		return -1;
	}
	p->status = PS_READY;

	p->stack_base = allocMemory(mm, PROCESS_STACK_SIZE);
	if (p->stack_base == NULL) {
		freeMemory(mm, p);
		return -1;
	}
	p->stack_pointer = p->stack_base + PROCESS_STACK_SIZE;

	// no se si hace falta o no duplicar el argv, chequear eso
	p->entry = entry;
	p->argv = duplicateArgv(argv, argc, mm);
	if (p->argv == NULL) {
        freeMemory(mm, p->stack_base);
        freeMemory(mm, p);
        return -1;
    }
	p->argc = argc;

	strncpy(p->name, name, MAX_NAME_LENGTH);

	processes[p->pid] = p;

	p->stack_pointer = setup_initial_stack(&process_caller, p->pid, p->stack_pointer);
	
	if (current_process >= 0 && processes[current_process] != NULL && processes[current_process]->status == PS_RUNNING) {
		processes[current_process]->status = PS_READY;
	}
	// run_next_process();
	return p->pid;

}

void proc_exit(int status) {
	(void)status;
	if (current_process < 0) return;
	PCB *p = processes[current_process];
	if (p == NULL) return;
	// Marcar terminado; NO liberar stack aquí porque estamos ejecutando sobre esa pila
	p->status = PS_TERMINATED;
	// Elegir próximo y pedir cambio de contexto inmediato
	run_next_process();
	void *next_rsp = switch_to_rsp;
	switch_to_rsp = 0;
	if (next_rsp != 0) {
		// Cambia a la pila del siguiente proceso y hace iretq; no retorna
		switch_to_rsp_and_iret(next_rsp);
	}
}

int  proc_getpid(void) {
	return current_process;
}

void proc_yield(void) {
	// Marcar actual como READY y pasar al siguiente READY (cooperativo)
	if (current_process >= 0 && processes[current_process] != NULL && processes[current_process]->status == PS_RUNNING) {
		processes[current_process]->status = PS_READY;
	}
	run_next_process();
}

void proc_print() {
	uint32_t color = 0xFFFFFF;
	for (int i = 0; i < MAX_PROCESSES; i++) {
		PCB *p = processes[i];
		if (p == NULL) {
			continue;
		}

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
			while (t > 0) {
				pid_buf[k++] = tmp[--t];
			}
		}
		pid_buf[k] = 0;

		vdPrint("id: ", color);
		vdPrint(pid_buf, color);
		vdPrint(" name: ", color);
		vdPrint(p->name, color);
		vdPrint(" state: ", color);

		switch (p->status) {
			case PS_RUNNING:    vdPrint("RUNNING", color);    break;
			case PS_READY:      vdPrint("READY", color);      break;
			case PS_TERMINATED: vdPrint("TERMINATED", color); break;
			default:            vdPrint("UNKNOWN", color);    break;
		}
		vdPrint("\n", color);
	}
}

// auxiliares

static int asign_pid() {
	if (current_process == -1) {
		return 0;
	}

	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (processes[i] == NULL) {
			return i;
		}
	}

	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (processes[i]->status == PS_TERMINATED) {
			return i;
		}
	}

	return -1;
}

static char **duplicateArgv(const char **argv, int argc, MemoryManagerADT mm) {
    if (argv == NULL || argv[0] == NULL) {
        char **new_argv = allocMemory(mm, sizeof(char *));
        if (new_argv == NULL)
            return NULL;
        new_argv[0] = NULL;
        return new_argv;
    }

    char **new_argv = allocMemory(mm,(argc + 1) * sizeof(char *));
    if (new_argv == NULL)
        return NULL;

    for (int i = 0; i < argc; i++) {
        int len = strlen(argv[i]) + 1;
        new_argv[i] = allocMemory(mm, len);
        if (new_argv[i] == NULL)
            return NULL;
        memcpy(new_argv[i], argv[i], len);
    }

    new_argv[argc] = NULL;
    return new_argv;
}

static void run_next_process();

void process_caller(int pid) {
	PCB * p = processes[pid];
	int res = p->entry(p->argc, p->argv);
	proc_exit(res);
}

// Devuelve el índice del siguiente proceso READY empezando en startIdx (inclusive),
// recorriendo circularmente. Retorna -1 si no hay ninguno.
static int pick_next_ready_from(int startIdx) {
	if (startIdx < 0) startIdx = 0;
	for (int k = 0; k < MAX_PROCESSES; k++) {
		int idx = (startIdx + k) % MAX_PROCESSES;
		if (processes[idx] != NULL && processes[idx]->status == PS_READY) {
			return idx;
		}
	}
	return -1;
}

static void run_next_process() {
	// Elegimos el siguiente READY distinto del actual, con política circular
	int start = current_process >= 0 ? (current_process + 1) % MAX_PROCESSES : 0;
	int next = pick_next_ready_from(start);
	if (next < 0) {
		// No hay a quién correr; no cambiamos contexto
		return;
	}

	// Guardar el kernel RSP actual en el PCB del proceso actual (si existe y es válido)
	// Nota: current_kernel_rsp solo es válido cuando venimos de una syscall (ISR)
	if (current_process >= 0 && processes[current_process] != NULL &&
		processes[current_process]->status != PS_TERMINATED && current_kernel_rsp != 0) {
		processes[current_process]->stack_pointer = current_kernel_rsp;
	}

	// Marcar RUNNING el próximo y fijarlo como actual
	processes[next]->status = PS_RUNNING;
	current_process = next;

	// Solicitar al handler de syscalls que cambie de contexto al volver a usuario
	// La pila del próximo ya está preparada (setup_initial_stack para primera ejecución
	// o la última pila de kernel salvada para reanudación)
	switch_to_rsp = processes[next]->stack_pointer;
}

static void free_process_resources(PCB *p) {
	MemoryManagerADT mm = getKernelMemoryManager();
	if (p->argv != NULL) {
		for (int i = 0; i < p->argc; i++) {
			if (p->argv[i] != NULL) {
				freeMemory(mm, p->argv[i]);
			}
		}
		freeMemory(mm, p->argv);
		p->argv = NULL;
	}
	if (p->stack_base != NULL) {
		freeMemory(mm, p->stack_base);
		p->stack_base = NULL;
		p->stack_pointer = NULL;
	}
	freeMemory(mm, p);
}


