#include <processes.h>
#include <memoryManager.h>
#include <string.h>

// TODO: despues pasar todo lo de scheduling a otro archivo scheduler.c y en lo posible hacerlo adt
// onda la idea seria que si tiene estado interno sea ADT y sino que sea tipo libreria de funciones 
// processes tiene sentido que sea una libreria de funciones y scheduler un ADT

static PCB * processes[MAX_PROCESSES] = {0};
static int current_process = -1;
// lo manejo como un coso circular non preemptive

// auxiliares
static int asign_pid();
static char **duplicateArgv(char **argv, int argc, MemoryManagerADT mm);
static void run_next_process();
static void process_caller(int pid);



void init_processes(); // este tiene que crear el proceso de la shell que va a ser el unico en principio

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
	p->argv = duplicateArgv(argv, argc, mm);
	if (p->argv == NULL) {
        freeMemory(mm, p->stack_base);
        freeMemory(mm, p);
        return -1;
    }
	p->argc = argc;

	strncpy(p->name, name, MAX_NAME_LENGTH);

	processes[p->pid] = p;

	p->stack_pointer = steup_stack_frame(&process_caller, p->pid, p->stack_pointer);
	// deja seteado con rdi para que cuando retorne vuelva a process_caller
	// no habria que hacer ninguna llamada a funcion de este punto en adelante
	return p->pid;

}

void proc_exit(int status);

int  proc_getpid(void);

void proc_yield(void);

// auxiliares

static int asign_pid() {
	if (current_process = -1) {
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

static char **duplicateArgv(char **argv, int argc, MemoryManagerADT mm) {
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


