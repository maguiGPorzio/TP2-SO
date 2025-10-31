#include "usrlib.h"

int cat_main(int argc, char * argv[]) {
    for (int i = 0; i < argc; i++) {
        print_string(argv[i]);
    }
    putchar(EOF);
    return 0;
}