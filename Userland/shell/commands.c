#include "../include/shell.h"
#include "programs.h"

#define MAX_ARGS 16

// ============================================
//           COMANDOS BUILTIN
// ============================================

typedef struct {
    char *name;
    void (*handler)(int argc, char **argv);
} BuiltinCommand;

static void builtin_clear(int argc, char **argv);
static void builtin_help(int argc, char **argv);

static BuiltinCommand builtins[] = {
    { "clear", &builtin_clear },
    { "help", &builtin_help },
    { NULL, NULL }
};

// ============================================
//           PROGRAMAS EXTERNOS
// ============================================

typedef struct {
    char *name;
    process_entry_t entry;
} ExternalProgram;

static ExternalProgram programs[] = {
    { "cat", &cat_main },
    { "red", &red_main },
    { "time", &time_main },
    { "date", &date_main },
    { "ps", &ps_main },
    { NULL, NULL }
};

// ============================================
//           PARSING Y EJECUCIÓN
// ============================================

static void parse_input(char *input, char **argv, int *argc) {
    *argc = 0;
    int in_token = 0;
    
    for (int i = 0; input[i] != '\0' && *argc < MAX_ARGS; i++) {
        if (input[i] == ' ') {
            if (in_token) {
                input[i] = '\0';
                in_token = 0;
            }
        } else {
            if (!in_token) {
                argv[(*argc)++] = &input[i];
                in_token = 1;
            }
        }
    }
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
    char *argv[MAX_ARGS];
    int argc = 0;
    
    parse_input(line, argv, &argc);
    
    if (argc == 0) {
        return;
    }
    
    char *command = argv[0];
    
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

    print("Available commands:\n\n");
    
    print("Builtin commands:\n");
    for (int i = 0; builtins[i].name != NULL; i++) {
        print("  ");
        print(builtins[i].name);
        putchar('\n');
    }
    
    print("\nExternal programs:\n");
    for (int i = 0; programs[i].name != NULL; i++) {
        print("  ");
        print(programs[i].name);
        putchar('\n');
    }
    
    

}
