#include "../include/shell.h"
#include "programs.h"

#define E5 659
#define B4 494
#define C5 523
#define D5 587
#define G4 392
#define F5 698
#define A4 440
#define A5 880
#define G5 784
#define E4 330
#define C4 262

// Duraciones (en milisegundos)
#define Q 400  // negra
#define E 200  // corchea
#define S 100  // semicorchea


// COMANDOS
static Command commands[] = {
    { "clear", "clears the screen",    &cls },
    { "help", "provides commands information", &help     },
    { "song", "plays tetris song", &song },
    { "print date", "prints system's date", &print_date },
    { "print regs", "prints the last saved register values", &print_saved_registers },
    { "print time", "prints system's time", &print_time },
    { "print processes", "prints current processes", &print_processes },
    { "test div0", "causes division by zero exception", &test_division_zero }, 
    { "test invopcode", "causes invalid opcode exception", &test_invalid_opcode }, 
    { "test mm", "runs a memory manager test", &test_mm_command }, 
    { "test processes", "runs processes test", &test_processes_command },
    { "test priority", "runs a priority test", &test_priority_command },
    { "test sync", "runs a synchronization test", &test_sync_command },
    { "test pipes", "runs cat hola | red", &test_pipes_command },
    { "cat", "runs cat hola manola", &cat_runner},
    { "red", "prints every input red until EOF", &red_runner},
    {0 ,0, 0} // marca de fin
};

static int clean_history;

static int get_command_idx(char * line) {

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(line, commands[i].name) == 0) {
            return i;
        }
    }

    return -1;
}


void process_line(char * line, uint32_t * history_len) {
    clean_history = 0;
    int idx = get_command_idx(line);

    if (idx < 0) {
        shell_print_err("Unknown command: '");
        shell_print_err(line);
        shell_print_err("'\n");
        shell_print_err(ERROR_MSG); 
        return;
    }

    commands[idx].fn();
    if (clean_history) {
        *history_len = 0;
    }
	
}

/*-- IMPLEMENTACION DE LOS COMANDOS --*/

void help() {
    shell_newline();
    shell_print_string(HELP_MSG); 
    shell_newline();
    for (Command *cmd = commands; cmd->name; ++cmd) {
        shell_print_string("    ");
        shell_print_string(cmd->name);
        shell_print_string(" - ");
        shell_print_string(cmd->description);
        shell_newline();
    }
    shell_newline();
}

void cls() { 
    sys_clear(); 
    clean_history = 1; 
}


void test_division_zero() {
    cls();
    int a = 1;
    int b = 0;
    a = a / b;  // Esto genera la excepción
}

void test_invalid_opcode(void) {
    cls();
    generate_invalid_opcode(); 
}

void print_saved_registers() {
    char buf[REGSBUF_SIZE];
    if(sys_regs(buf)) {
        shell_print_string(buf);
    } else {
        shell_print_err("No register snapshot available. Press left ctrl to take one\n");
    }
}

// funcion helper para horas y fechas
static void print_formatted_data(void (*syscall_fn)(uint8_t*), char separator) {
    uint8_t buf[3];
    syscall_fn(buf);
    char number_buf[4];
    char output_buf[10]; // xx:xx:xx\n\0 o dd/mm/yy\n\0
    
    for (int i = 0; i < 3; i++) {
        uint64_t digits = num_to_str(buf[i], number_buf, 16);
        if (digits == 1) {
            output_buf[3*i] = '0';
            output_buf[3*i + 1] = number_buf[0];
        } else {
            output_buf[3*i] = number_buf[0];
            output_buf[3*i + 1] = number_buf[1];
        }
        output_buf[3*i + 2] = separator;
    }
    output_buf[8] = '\n';
    output_buf[9] = 0;
    shell_print_string(output_buf);
}

void print_time() {
    print_formatted_data(sys_time, ':');
}

void print_date() {
    print_formatted_data(sys_date, '/');
}

void song() {
    sys_beep(1320, 500);
    sys_beep(990, 250);
    sys_beep(1056, 250);
    sys_beep(1188, 250);
    sys_beep(1320, 125);
    sys_beep(1188, 125);
    sys_beep(1056, 250);
    sys_beep(990, 250);
    sys_beep(880, 500);
    sys_beep(880, 250);
    sys_beep(1056, 250);
    sys_beep(1320, 500);
    sys_beep(1188, 250);
    sys_beep(1056, 250);
    sys_beep(990, 750);
    sys_beep(1056, 250);
    sys_beep(1188, 500);
    sys_beep(1320, 500);
    sys_beep(1056, 500);
    sys_beep(880, 500);
    sys_beep(880, 500);
    sys_clear_input_buffer();
    shell_newline();
}

// Llama al test de memory manager con un límite predeterminado
void test_mm_command() {
    static char * args[] = { "20000" }; // tamaño máximo configurable aquí
    test_mm(1, args);
}

void test_processes_command() {
    const char * args[] = {"4"};
    sys_create_process(&test_processes, 1, args, "test_processes", NULL);
}

void test_priority_command() {
    putchar('\b'); // borro el cursor
    const char * args[] = {"600000000"}; // tiene que ser un numero grande para que es note la dif
    int pid = sys_create_process(&test_prio, 1, args, "test_prio", NULL);
    sys_wait(pid);
    shell_newline();
}


void print_processes() {
    putchar('\b'); // borro el cursor
    sys_print_processes();
    shell_newline();
}

void test_sync_command() {
    putchar('\b'); // borro el cursor
    const char *args[] = {"1000", "1", NULL};
    int pid = sys_create_process(&test_sync, 2, args, "test_sync", NULL);
    sys_wait(pid);
    shell_newline();
}

void test_pipes_command() {
    putchar('\b'); // borro el cursor
    char c = 'a';
    for (int i = STDOUT; i <= STDYELLOW; i++) {
        fprint(i, "locura\n");
    }

    print("running: 'cat hola | red'\n");

    int fds_pipe[2];
    int pipe_id = sys_create_pipe(fds_pipe);
    if (pipe_id == -1) {
        print_err("failed to create pipe");
        shell_newline();
        return;
    }

    printf("pipe: %d, fds: %d y %d\n", pipe_id, fds_pipe[0], fds_pipe[1]);

    int fds_cat[2];
    fds_cat[0] = STDIN;
    fds_cat[1] = fds_pipe[1];

    int fds_red[2];
    fds_red[0] = fds_pipe[0];
    fds_red[1] = STDOUT;

    const char * args[] = {"hola", NULL};
    int pid_cat = sys_create_process(&cat_main, 1, args, "cat", fds_cat);
    int pid_red = sys_create_process(&red_main, 1, args, "red", fds_red);

    sys_wait(pid_cat);
    sys_wait(pid_red);
    sys_destroy_pipe(pipe_id);

    putchar('\n');
    shell_newline();
}

void cat_runner() {
    putchar('\b'); // borro el cursor
    const char *args[] = {"hola ", "manola", NULL};
    int pid = sys_create_process(&cat_main, 2, args, "cat", NULL);
    sys_wait(pid);
    putchar('\n');
    shell_newline();

}

void red_runner() {
    putchar('\b'); // borro el cursor
    const char *args[] = {"hola ", "manola", NULL};
    int pid = sys_create_process(&red_main, 2, args, "red", NULL);
    sys_wait(pid);
    shell_newline();
}


