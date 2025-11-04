#include "usrlib.h"
#include "test_mm.h"
#include "test_sync.h"

// Prototipos de tests disponibles desde la shell
int64_t test_prio(uint64_t argc, char *argv[]);
int64_t test_processes(uint64_t argc, char *argv[]);

#define INPUT_MAX 128
#define PROMPT "> "
#define CURSOR '_'
#define ERROR_MSG "Use command \'help\' to see available commands\n"

// funciones de la shell  
void read_line(char * buf, uint64_t max);
void process_line(char * line); 
void incfont();
void decfont();






