#include "usrlib.h"

static int get_next_fd(int fd);

int rainbow_main(int argc, char * argv[]) {
    char c;
    int fd = STDOUT;
    while ((c = getchar()) != EOF) {

        sys_write(fd, &c, 1);
        fd = get_next_fd(fd);

    }

    return 0;
}

static int get_next_fd(int fd) {
    return fd+1 >= FDS_COUNT ? STDOUT : fd+1;
}