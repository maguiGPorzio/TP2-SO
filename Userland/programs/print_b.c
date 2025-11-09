// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "usrlib.h"

int print_b_main(int argc, char *argv[]) {
    while (1) {
        fprint(STDOUT, "b");
        //sys_sleep(1000); 
    }
}