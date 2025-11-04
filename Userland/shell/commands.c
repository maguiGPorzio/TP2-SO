#include "../include/shell.h"
#include "programs.h"
#include "tests.h"

#define MAX_ARGS 16

// ============================================
//           COMANDOS BUILTIN
// ============================================


static void cls(int argc, char * argv[]);
static void help(int argc, char * argv[]);
static void test_runner(int argc, char * argv[]);
static void print_test_use();

static BuiltinCommand builtins[] = {
    { "clear", "clears the screen", &cls },
    { "help", "provides information about available commands", &help },
    { "test", "runs a test, run 'test' to see more information about its use", &test_runner },

    { NULL, NULL, NULL }
};

// ============================================
//           PROGRAMAS EXTERNOS
// ============================================



static ExternalProgram programs[] = {
    { "cat", "prints to STDOUT its params", &cat_main },
    { "red", "reads from STDIN and prints it to STDERR", &red_main },
    { "rainbow", "reads from STDIN and prints one char to each color fd", &rainbow_main},
    { "time", "prints system time to STDOUT", &time_main },
    { "date", "prints system date to STDOUT",&date_main },
    { "ps", "prints to STDOUT information about current processes",&ps_main },

    { NULL, NULL, NULL } // TODO: ver si lo hacemos null terminated o con un define del length
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

static int try_external_program(char *name, int argc, char **argv) {
    process_entry_t entry = find_program_entry(name);
    
    if (entry == NULL) {
        return 0;
    }
    
    int pid = sys_create_process(
        entry,
        argc,
        (const char **)argv,
        name,
        NULL
    );
    
    if (pid < 0) {
        print_err("Failed to create process\n");
        return 0;
    }
    
    sys_wait(pid);
    putchar('\n');
    return 1;
}

// Ejecuta dos comandos conectados por pipe: left_cmd | right_cmd
static int execute_piped_commands(char **left_tokens, int left_count, 
                                    char **right_tokens, int right_count) {
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
    int pid_left = sys_create_process(
        left_entry,
        left_argc,
        (const char **)left_argv,
        left_cmd,
        fds_left
    );
    
    int pid_right = sys_create_process(
        right_entry,
        right_argc,
        (const char **)right_argv,
        right_cmd,
        fds_right
    );
    
    if (pid_left < 0 || pid_right < 0) {
        print_err("Failed to create piped processes\n");
        sys_destroy_pipe(pipe_id);
        return 0;
    }
    
    // Esperar a que terminen ambos procesos
    sys_wait(pid_left);
    sys_wait(pid_right);
    
    // Destruir el pipe
    sys_destroy_pipe(pipe_id);
    
    putchar('\n');
    return 1;
}

void process_line(char *line) {
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
        
        // Ejecutar con pipe
        execute_piped_commands(left_tokens, left_count, right_tokens, right_count);
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
    if (try_external_program(command, argc, argv)) {
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
    
    print("\nExternal programs:\n");
    for (int i = 0; programs[i].name != NULL; i++) {
        print("  ");
        print(programs[i].name);
        print(" - ");
        print(programs[i].description);
        putchar('\n');
    }

    putchar('\n');
}

static void test_runner(int argc, char * argv[]) {
    if (argc == 0) {
        print_err("Error: missing test name\n");
        print_test_use();
        return;
    }
    
    char *test_name = argv[0];
    char **test_argv = &argv[1];  // Argumentos para el test (si los hay)
    int test_argc = argc - 1;     // Número de argumentos para el test
    
    process_entry_t test_entry = NULL;
    
    // Determinar qué test ejecutar
    if (strcmp(test_name, "mm") == 0) {
        test_entry = (process_entry_t)test_mm;
    } else if (strcmp(test_name, "prio") == 0) {
        test_entry = (process_entry_t)test_prio;
    } else if (strcmp(test_name, "processes") == 0) {
        test_entry = (process_entry_t)test_processes;
    } else if (strcmp(test_name, "sync") == 0) {
        test_entry = (process_entry_t)test_sync;
    } else {
        print_err("Error: unknown test '");
        print_err(test_name);
        print_err("'\n");
        print_test_use();
        return;
    }
    
    // Crear y ejecutar el proceso del test
    int pid = sys_create_process(
        test_entry,
        test_argc,
        (const char **)test_argv,
        test_name,
        NULL
    );
    
    if (pid < 0) {
        print_err("Error: failed to create test process\n");
        return;
    }
    
    sys_wait(pid);
    putchar('\n');
}

static void print_test_use() {
    print("Use: test <test_name> [test_params]\n");
    print("Available test names:\n");
    print("  mm         - memory manager test\n");
    print("  prio       - priority scheduling test\n");
    print("  processes  - process management test\n");
    print("  sync       - synchronization test\n");
}
