#include "usrlib.h"

// cuenta lineas, palabras y caracteres que recibe por stdin
int wc_main(int argc, char *argv[]) {
    char c;
    int lines = 0;
    int words = 0;
    int chars = 0;
    int in_word = 0;  // flag para saber si estamos dentro de una palabra
    
    while ((c = getchar()) != EOF) {
        chars++;
        
        if (c == '\n') {
            lines++;
        }
        
        // Una palabra es una secuencia de caracteres no-espacio
        if (c == ' ' || c == '\t' || c == '\n') {
            if (in_word) {
                words++;
                in_word = 0;
            }
        } else {
            in_word = 1;
        }
    }
    
    // Si terminamos dentro de una palabra (sin newline final), contarla
    if (in_word) {
        words++;
    }
    
    printf("%d line%s, %d word%s, %d character%s\n", 
           lines, lines == 1 ? "" : "s",
           words, words == 1 ? "" : "s",
           chars, chars == 1 ? "" : "s");
    
    return 0;
}