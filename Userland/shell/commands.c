#include "../include/shell.h"
#include "programs.h"

#define MAX_ARGS 16

// ============================================
//           COMANDOS BUILTIN
// ============================================


static void builtin_clear(int argc, char **argv);
static void builtin_help(int argc, char **argv);

static BuiltinCommand builtins[] = {
    { "clear", "clears the screen", &builtin_clear },
    { "help", "provides information about available commands", &builtin_help },
    { NULL, NULL }
};

// ============================================
//           PROGRAMAS EXTERNOS
// ============================================



static ExternalProgram programs[] = {
    { "cat", "prints to STDOUT its params", &cat_main },
    { "red", "reads from STDIN and prints it to STDERR", &red_main },
    { "time", "prints system time to STDOUT", &time_main },
    { "date", "prints system date to STDOUT",&date_main },
    { "ps", "prints to STDOUT information about current processes",&ps_main },
    { NULL, NULL }
};

// ============================================
//           PARSING Y EJECUCIÓN
// ============================================

// Parsea el input y devuelve el número de tokens encontrados
// tokens[0] = comando, tokens[1..n] = argumentos
static int parse_input(char *input, char **tokens) {
    int count = 0;
    int in_token = 0;  // Flag: indica si estamos dentro de una palabra
    
    for (int i = 0; input[i] != '\0' && count < MAX_ARGS; i++) {
        if (input[i] == ' ') {
            if (in_token) {
                input[i] = '\0';  // terminar la palabra actual
                in_token = 0;
            }
        } else {
            if (!in_token) {
                tokens[count++] = &input[i];  // guardar inicio de nueva palabra
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

static int try_external_program(char *name, int argc, char **argv) {
    for (int i = 0; programs[i].name != NULL; i++) {
        if (strcmp(name, programs[i].name) == 0) {
            int pid = sys_create_process(
                programs[i].entry,
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
    }
    return 0;
}

void process_line(char *line) {
    char *tokens[MAX_ARGS];
    int token_count = parse_input(line, tokens);
    
    if (token_count == 0) {
        return;
    }
    
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

static void builtin_clear(int argc, char **argv) {
    sys_clear();
}

static void builtin_help(int argc, char **argv) {

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
