#include "usrlib.h"

int print_a_main(int argc, char *argv[]) {
    while (1) {
        print("a");
        sys_sleep(1000); 
    }
}