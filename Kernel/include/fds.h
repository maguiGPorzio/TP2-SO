#ifndef FDS_H
#define FDS_H

#include <stdint.h>

// File descriptors estándar
enum {
    STDIN = 0,
    STDOUT,
    STDERR,
    STDGREEN,
    STDBLUE,
    STDCYAN,
    STDMAGENTA,
    STDYELLOW,
    FIRST_FREE_FD
};

// Declaración externa: el array se define en fds.c
extern uint32_t fd_colors[];

#endif
