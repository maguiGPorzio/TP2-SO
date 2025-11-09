#include "usrlib.h"

int print_a_main(int argc, char *argv[]) {
    while (1) {
        fprint(STDOUT, "a");
        sys_sleep(1000); 
    }
}