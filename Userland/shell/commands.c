#include "../include/shell.h"


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

#define COMMAND_COUNT 11

// COMANDOS
static Command commands[] = {
    { "clear", "clears the screen",    cls      },
    { "help", "provides commands information",     help     },
    { "print date", "prints system's date", print_date },
    { "print regs", "prints the last saved register values", print_saved_registers },
    { "print time", "prints system's time", print_time },
    { "spawn a", "runs a process that writes a to stdout", test_spawn_a }, // prueba de procesos: imprime 'a'
    { "song", "plays tetris song", song },
    { "test div0", "causes division by zero exception", test_division_zero }, // prueba de división por cero
    { "test invopcode", "causes invalid opcode exception", test_invalid_opcode }, // prueba de opcode inválido
    { "test mm", "runs a memory manager test", test_mm_command }, // prueba del memory manager
    {0,0}
};

static int clean_history;

static int binary_search_command(char * line) {
    int left = 0;
    int right = COMMAND_COUNT - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(line, commands[mid].name);
        
        if (cmp == 0) {
            return mid; 
        } else if (cmp < 0) {
            right = mid - 1; 
        } else {
            left = mid + 1; 
        }
    }
    return -1; 
}

void process_line(char * line, uint32_t * history_len) {
    clean_history = 0;
    int idx = binary_search_command(line);

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

void cls()      { 
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
}

// Llama al test de memory manager con un límite predeterminado
void test_mm_command() {
    static char * args[] = { "20000" }; // tamaño máximo configurable aquí
    test_mm(1, args);
}

// ====== Nuevo comando: spawnea un proceso que imprime 'a' ======
extern int proc_print_a(int argc, char **argv);

void test_spawn_a() {
    const char *args[] = { "a" };
    int64_t pid = sys_spawn(proc_print_a, 1, args, "proc_a");
    if (pid < 0) {
        shell_print_err("spawn fallo\n");
        return;
    }
    // Ceder CPU para que el proceso corra (scheduler cooperativo)
    sys_yield();
}