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
