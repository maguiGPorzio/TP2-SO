#include "usrlib.h"

int cat_main(int argc, char * argv[]) {
    char c;
    while ((c = getchar()) != EOF) {
        sys_write(STDOUT, &c, 1);
    }

    return OK;
}