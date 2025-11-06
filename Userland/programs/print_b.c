#include "usrlib.h"

int print_b_main(int argc, char *argv[]) {
    while (1) {
        print("b");
        sys_sleep(1000); 
    }
}