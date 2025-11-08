#include "../include/shell.h"
#include "programs.h"
#include "tests.h"
#include <stdbool.h>
#include "syscalls.h"

#define NO_PID -1 // TODO: ver esto donde lo ponemos porque lo comparte con Kernel
#define MAX_ARGS 16

// ============================================
//           COMANDOS BUILTIN
// ============================================


static void cls(int argc, char * argv[]);
static void help(int argc, char * argv[]);
static void list_pipes(int argc, char * argv[]);
static void kill_process(int argc, char * argv[]);

static bool is_cmd_background(char *line);

static BuiltinCommand builtins[] = {
    { "clear", "clears the screen", &cls },
    { "help", "provides information about available commands", &help },
    { "pipes", "lists active pipes information", &list_pipes },
    { "kill", "kills a process by PID", &kill_process },

    { NULL, NULL, NULL }
};

// ============================================
//           PROGRAMAS EXTERNOS
// ============================================



static ExternalProgram programs[] = {
    { "echo", "prints to STDOUT its params", &echo_main },
    { "cat", "reads from STDIN and prints it to STDOUT", &cat_main },
    { "red", "reads from STDIN and prints it to STDERR", &red_main },
    { "rainbow", "reads from STDIN and prints one char to each color fd", &rainbow_main},
    { "time", "prints system time to STDOUT", &time_main },
    { "date", "prints system date to STDOUT", &date_main },
    { "ps", "prints to STDOUT information about current processes", &ps_main },
    { "printa", "prints the letter 'a' indefinitely to STDOUT", &print_a_main },
    { "printb", "prints the letter 'b' indefinitely to STDOUT", &print_b_main },
    { "text", "lo puse para probar wc", &text_main },
    { "loop", "runs an infinite loop printing dots to STDOUT", &loop_main },
    { "nice", "changes the priority of a process", &nice_main },
    { "wc", "counts the number of lines, words and characters from STDIN", &wc_main },
    { "mem", "prints memory usage information", &mem},
    { "block", "blocks a running process", &block}, 
    { "unblock", "unblocks a blocked process", &unblock},
    { "filter", "filters out vowels from input until '-' is encountered", &filter},
    { "mvar", "tests multi-variable synchronization. Run mvar <amount_writers> <amount_readers>", &mvar_main},
    { "test_mm", "runs an mm test", &test_mm},
    { "test_prio", "runs a priority test", &test_prio},
    { "test_processes", "runs an process test", &test_processes},
    { "test_sync", "runs a sync test.\n              run test_sync <number_increments/decrements_per_process> <use_semaphore>.\n              to use semaphores write 1 in <use_semaphore> else 0", &test_sync},
    { "test_pipes", "runs a piptes test", &test_pipes},
    { "mvar", "tests multi-variable synchronization", &mvar_main},
    { NULL, NULL }
};



// ============================================
//           PARSING Y EJECUCIÓN
// ============================================

// Busca el operador '|' en los tokens y retorna su índice, o -1 si no existe
static int find_pipe_operator(char **tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        // El token del pipe será un string que empieza con '|'
        if (tokens[i][0] == '|') {
            return i;
        }
    }
    return -1;
}

// Parsea el input y devuelve el número de tokens encontrados
// tokens[0] = comando, tokens[1..n] = argumentos
// Reconoce '|' como delimitador especial
static int parse_input(char *input, char **tokens) {
    int count = 0;
    int in_token = 0;
    
    for (int i = 0; input[i] != '\0' && count < MAX_ARGS; i++) {
        char c = input[i];
        
        if (c == ' ' || c == '|') {
            // Terminar token actual si estábamos en uno
            if (in_token) {
                input[i] = '\0';
                in_token = 0;
            }
            
            // Si es pipe, guardarlo como token
            if (c == '|') {
                tokens[count++] = &input[i];
                // Mover a siguiente posición
                i++;
                // Saltar espacios después del pipe
                while (input[i] == ' ') {
                    input[i] = '\0';
                    i++;
                }
                i--; // Compensar el i++ del for
            }
        } else {
            // Estamos en un carácter normal
            if (!in_token) {
                tokens[count++] = &input[i];
                in_token = 1;
            }
        }
    }
    
    return count;
}

static int try_builtin_command(char *name, int argc, char **argv) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            builtins[i].handler(argc, argv);
            return 1;
        }
    }
    return 0;
}

// Encuentra el entry point de un programa externo. Retorna NULL si no existe.
static process_entry_t find_program_entry(char *name) {
    for (int i = 0; programs[i].name != NULL; i++) {
        if (strcmp(name, programs[i].name) == 0) {
            return programs[i].entry;
        }
    }
    return NULL;
}

static int try_external_program(char *name, int argc, char **argv, bool background) {
    process_entry_t entry = find_program_entry(name);
    
    if (entry == NULL) {
        return 0;
    }
    
    int pid = sys_create_process(entry, argc, (const char **)argv, name, NULL);
    
    if (pid < 0) {
        print_err("Failed to create process\n");
        return 0;
    }

    if (background) {
        sys_adopt_init_as_parent(pid); // los hago huerfanos para que cuando terminen se liberen solos
        return 1;
    } 
        
    // Sigue acá si es foreground
    sys_set_foreground_process(pid);
    sys_wait(pid);
    sys_clear_input_buffer(); // limpiar buffer de entrada por si quedó algo
    putchar('\n');
    return 1;
}

// Ejecuta dos comandos conectados por pipe: left_cmd | right_cmd
static int execute_piped_commands(char **left_tokens, int left_count, 
                                    char **right_tokens, int right_count, bool background) {
    // Validar que ambos comandos existen
    char *left_cmd = left_tokens[0];
    char *right_cmd = right_tokens[0];
    
    process_entry_t left_entry = find_program_entry(left_cmd);
    process_entry_t right_entry = find_program_entry(right_cmd);
    
    if (left_entry == NULL) {
        print_err("Unknown program: '");
        print_err(left_cmd);
        print_err("'\n");
        return 0;
    }
    
    if (right_entry == NULL) {
        print_err("Unknown program: '");
        print_err(right_cmd);
        print_err("'\n");
        return 0;
    }
    
    // Crear pipe
    int fds_pipe[2];
    int pipe_id = sys_create_pipe(fds_pipe);
    if (pipe_id < 0) {
        print_err("Failed to create pipe\n");
        return 0;
    }
    
    // Preparar argumentos (sin contar el comando)
    char **left_argv = &left_tokens[1];
    int left_argc = left_count - 1;
    
    char **right_argv = &right_tokens[1];
    int right_argc = right_count - 1;
    
    // FDs para el comando izquierdo (escribe al pipe)
    int fds_left[2];
    fds_left[0] = STDIN;          // Lee de STDIN
    fds_left[1] = fds_pipe[1];    // Escribe al pipe (write end)
    
    // FDs para el comando derecho (lee del pipe)
    int fds_right[2];
    fds_right[0] = fds_pipe[0];   // Lee del pipe (read end)
    fds_right[1] = STDOUT;        // Escribe a STDOUT
    
    // Crear ambos procesos
    int pid_left = sys_create_process(left_entry, left_argc, (const char **)left_argv, left_cmd, fds_left);
    
    int pid_right = sys_create_process(right_entry, right_argc, (const char **)right_argv, right_cmd, fds_right);

    sys_close_fd(fds_pipe[0]);
    sys_close_fd(fds_pipe[1]);
    
    if (pid_left < 0 || pid_right < 0) {
        print_err("Failed to create piped processes\n");
        sys_destroy_pipe(pipe_id);
        return 0;
    }
    
    if (background) {
        // Background: hacer huérfanos a ambos procesos
        sys_adopt_init_as_parent(pid_left);
        sys_adopt_init_as_parent(pid_right);
        return 1;
    }
    
    // Foreground: establecer el último proceso como foreground para poder matarlo con Ctrl+C
    sys_set_foreground_process(pid_right);
    
    // Esperar a que terminen ambos procesos
    sys_wait(pid_left);
    sys_wait(pid_right);
    sys_clear_input_buffer(); // limpiar buffer de entrada por si quedó algo
    putchar('\n');
    
    // Destruir el pipe
    sys_destroy_pipe(pipe_id);
    return 1;
}

static bool is_cmd_background(char *line) {
    bool background = false;

    int len = strlen(line);
    int i = len - 1;

    // salteo espacios finales
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t' || line[i] == '\n')) { 
        i--;
    }
    if (i >= 0 && line[i] == '&') {
        background = true;
        line[i] = '\0'; // quitar el '&' para que no interfiera con el parsing
    }
    return background;
}


void process_line(char *line) {
    bool background = is_cmd_background(line);

    char *tokens[MAX_ARGS];
    int token_count = parse_input(line, tokens);
    
    if (token_count == 0) {
        return;
    }
    
    // Buscar si hay un operador pipe '|'
    int pipe_idx = find_pipe_operator(tokens, token_count);
    
    if (pipe_idx != -1) {
        // HAY PIPE: dividir en dos comandos
        
        // Validar sintaxis básica
        if (pipe_idx == 0) {
            print_err("Syntax error: pipe at start of command\n");
            return;
        }
        if (pipe_idx == token_count - 1) {
            print_err("Syntax error: pipe at end of command\n");
            return;
        }
        
        // Comando izquierdo: tokens[0..pipe_idx-1]
        char **left_tokens = &tokens[0];
        int left_count = pipe_idx;
        
        // Comando derecho: tokens[pipe_idx+1..token_count-1]
        char **right_tokens = &tokens[pipe_idx + 1];
        int right_count = token_count - pipe_idx - 1;
        
        // Ejecutar con pipe (pasando el flag background)
        execute_piped_commands(left_tokens, left_count, right_tokens, right_count, background);
        return;
    }
    
    // NO HAY PIPE: ejecución normal
    char *command = tokens[0];
    char **argv = &tokens[1];      // argv[0] es el primer argumento
    int argc = token_count - 1;    // argc no cuenta el comando
    
    // Primero buscar en builtins
    if (try_builtin_command(command, argc, argv)) {
        return;
    }
    
    // Luego buscar en programas externos
    if (try_external_program(command, argc, argv, background)) {
        return;
    }
    
    // No se encontró
    print_err("Unknown command: '");
    print_err(command);
    print_err("'\n");
    print_err(ERROR_MSG);
}

// ============================================
//      IMPLEMENTACIÓN DE BUILTINS
// ============================================

static void cls(int argc, char * argv[]) {
    sys_clear();
}

static void help(int argc, char * argv[]) {

    print("\nType '+' or '-' to change font size\n\n");
    
    print("Builtin commands:\n");
    for (int i = 0; builtins[i].name != NULL; i++) {
        print("  ");
        print(builtins[i].name);
        print(" - ");
        print(builtins[i].description);
        putchar('\n');
    }
    putchar('\n');
    
    print("\nExternal programs:\n");
    print("--Type <program_name> & to run in background, else it runs in foreground--\n");
    for (int i = 0; programs[i].name != NULL; i++) {
        print("  ");
        print(programs[i].name);
        print(" - ");
        print(programs[i].description);
        putchar('\n');
    }

    putchar('\n');
}

static void list_pipes(int argc, char * argv[]) {
    sys_list_pipes();
}

static void kill_process(int argc, char * argv[]) {
    if (argc == 0) {
        print_err("Error: kill requires a PID\n");
        print_err("Usage: kill <pid>\n");
        return;
    }
    
    if (argc > 1) {
        print_err("Error: kill takes exactly one argument\n");
        print_err("Usage: kill <pid>\n");
        return;
    }
    
    // Parsear el PID desde el string usando satoi
    char *pid_str = argv[0];
    int pid = satoi(pid_str);
    
    // Llamar la syscall para matar el proceso
    int result = sys_kill(pid);

    // Imprimir mensajes específicos según el código de error
    switch (result) {
        case -1:
            print_err("[ERROR] Scheduler not initialized\n");
            break;
        case -2:
            print_err("[ERROR] Invalid PID\n");
            break;
        case -3:
            print_err("[ERROR] No process found with that PID\n");
            break;
        case -4:
            print_err("[ERROR] Cannot kill process: protected process (init or shell)\n");
            break;
    }
}

// static void print_test_use() {
//     print("Use: test <test_name> [test_params]\n");
//     print("Available test names:\n");
//     print("  mm         - memory manager test\n");
//     print("  prio       - priority scheduling test\n");
//     print("  processes  - process management test\n");
//     print("  sync       - synchronization test\n");
// }