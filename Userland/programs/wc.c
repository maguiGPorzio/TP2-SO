#include "usrlib.h"

// cuenta la cantidad de lineas que recibe por stdin
int wc_main(int argc, char *argv[]) {
    char c;
    int lines = 0;
    while ((c = getchar()) != EOF) {
        if (c == '\n') {
            lines++;
        }
    }
    printf("%d line%s in total\n", lines, lines == 1 ? "" : "s");
    return lines;
}