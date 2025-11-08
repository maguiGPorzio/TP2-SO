#include "usrlib.h"

int echo_main(int argc, char * argv[]) {
    for (int i = 0; i < argc; i++) {
        print(argv[i]);
        putchar(' ');
    }
    putchar(EOF);
    return OK;
}