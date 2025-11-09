#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

typedef void (*Exception)(void);


void exception_dispatcher(int exception);
static void excep_handler(char * msg);
static void zero_division();
static void invalid_opcode();

#endif