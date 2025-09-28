#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

typedef void (*Exception)(void);


void exceptionDispatcher(int exception);
static void excepHandler(char * msg);
static void zero_division();
static void invalid_opcode();

#endif