#include "usrlib.h"
#include "test_mm.h"
#include "test_sync.h"

// Prototipos de tests disponibles desde la shell
int64_t test_prio(uint64_t argc, char *argv[]);
int64_t test_processes(uint64_t argc, char *argv[]);

#define INPUT_MAX 100
#define HISTORY_MAX 4096 
#define PROMPT "> "
#define CURSOR '_'
#define ERROR_MSG "Use command \'help\' to see available commands\n"
#define HELP_MSG "* To change font size press + or -\n* Available commands:\n"

typedef struct {
    char character;
    uint64_t fd; // STDOUT o STDERR
} HistoryEntry;

typedef void (*Runnable)(void);

typedef struct Command {
    char *name;
    char *description;
    Runnable fn;
} Command;

// funciones de la shell
void shell_putchar(char c, uint64_t fd); 
void shell_print_string(char *str);
void shell_print_err(char *str);    
void shell_newline();  
void read_line(char * buf, uint64_t max);
void process_line(char * line, uint32_t * history_len); 
void incfont();
void decfont();
void redraw_history();

// comandos
void help();
void cls();
void test_division_zero();
void test_invalid_opcode();
void print_saved_registers();
void print_time();
void print_date();
void song();
void test_mm_command();
void test_processes_command();
void test_priority_command();
void print_processes();
void test_sync_command();

