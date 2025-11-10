// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "fds.h"

uint32_t fd_colors[] = {
        0x000000, // STDIN (no se usa para escritura)
        0xFFFFFF, // STDOUT - white
        0xFF0000, // STDERR - red
        0x00FF00, // STDGREEN - green
        0x0000FF, // STDBLUE - blue
        0x00FFFF, // STDCYAN - cyan
        0xFF00FF, // STDMAGENTA - magenta
        0xFFFF00, // STDYELLOW - yellow
};
