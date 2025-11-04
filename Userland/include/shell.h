#include "usrlib.h"

typedef void (*command_handler_t)(int argc, char **argv);

typedef struct {
    char *name;
    char * description;
    command_handler_t handler;

} BuiltinCommand;

typedef struct {
    char *name;
    char * description;
    process_entry_t entry;
} ExternalProgram;




#define INPUT_MAX 128
#define PROMPT "> "
#define CURSOR '_'
#define ERROR_MSG "Use command \'help\' to see available commands\n"

// funciones de la shell  
void read_line(char * buf, uint64_t max);
void process_line(char * line); 
void incfont();
void decfont();






