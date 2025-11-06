#include "usrlib.h"

int mvar_main(int argc, char *argv[]) {
    if(argc != 2){
        print_err("Usage: mvar <number of writers> <number of readers>\n");
        return -1;
    }

}