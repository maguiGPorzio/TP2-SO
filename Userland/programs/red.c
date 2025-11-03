#include "usrlib.h"

int red_main(int argc, char * argv[]) {
    char c;
    while ((c = getchar()) != EOF) {
        sys_write(STDERR, &c, 1);
    }

    return 0;
}