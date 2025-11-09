#include "../include/shell.h"

// Buffer para el comando actual y el anterior
static char current_input[INPUT_MAX];
static char previous_input[INPUT_MAX];
static char initial_input[INPUT_MAX];
static void print_initial_message();

int main(void) {
    sys_enable_textmode();
    
    // Limpiar buffer de teclado y resetear semáforo antes de empezar
    sys_clear_input_buffer();

    print_initial_message();

    // Limpiar buffer de teclado y resetear semáforo antes de empezar
    sys_clear_input_buffer();

    while (1) {
        fprint(STDCYAN, initial_input);
        print(PROMPT);
        read_line(current_input, INPUT_MAX-1);
        putchar('\n');
        process_line(current_input);
        // Guardar comando actual como anterior para próxima iteración
        for (int i = 0; i < INPUT_MAX; i++) {
            previous_input[i] = current_input[i];
            if (current_input[i] == '\0') break;
        }
    }
}


/*-- FUNCIONES AUXILIARES --*/
static void print_initial_message(){
    sys_increase_fontsize();
    fprint(STDMAGENTA, INITIAL_MESSAGE_1);
    putchar('\n');
    print(INITIAL_MESSAGE_2);
    read_line(initial_input, USERNAME_MAX_LENGTH);
    putchar('\n');
    fprint(STDMAGENTA, HELP_MESSAGE);
    putchar('\n');
    sys_decrease_fontsize();
}

void read_line(char * buf, uint64_t max) {
    char c;
    uint32_t idx = 0;
    
    // Limpiar el buffer al inicio
    for (int i = 0; i <= max; i++) {
        buf[i] = '\0';
    }
    
    // Mostrar cursor inicial
    putchar(CURSOR);

    while((c = getchar()) != '\n') {
        if (c == '+') {
            incfont();
        } else if(c == '-') {
            decfont();
        } else if (c == '\b') { // Backspace
            if (idx != 0) {
                idx--;
                buf[idx] = '\0'; // Limpiar el caracter borrado
                putchar('\b'); // borro el cursor
                putchar('\b'); // borro el caracter
                putchar(CURSOR);
            }
        } else if (idx < max) {
            buf[idx++] = c;
            putchar('\b'); // borro el cursor
            putchar(c);    // escribo el caracter
            putchar(CURSOR); // escribo el cursor
        }
    }

    putchar('\b'); // borro el cursor final antes de salir
    buf[idx] = 0;
}


void incfont()  { 
    sys_increase_fontsize(); 
    sys_clear();
    // Imprimir prompt sin cursor (usar print directamente)
    fprint(STDCYAN, initial_input);
    print(PROMPT);
    // Imprimir el input actual
    for (int i = 0; current_input[i] != '\0' && i < INPUT_MAX; i++) {
        putchar(current_input[i]);
    }
    // Un solo cursor al final
    putchar(CURSOR);
}

void decfont()  { 
    sys_decrease_fontsize(); 
    sys_clear();
    // Imprimir prompt sin cursor (usar print directamente)
    fprint(STDCYAN, initial_input);
    print(PROMPT);
    // Imprimir el input actual
    for (int i = 0; current_input[i] != '\0' && i < INPUT_MAX; i++) {
        putchar(current_input[i]);
    }
    // Un solo cursor al final
    putchar(CURSOR);
}

