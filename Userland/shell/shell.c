#include "../include/shell.h"

// historial para redibujar
static HistoryEntry history[HISTORY_MAX];
static uint32_t history_len = 0;


int main(void) {
    sys_enable_textmode();

    char input[INPUT_MAX];
    while (1) {
        shell_print_string(PROMPT);
        read_line(input, INPUT_MAX-1);
        shell_newline();
        putchar('\b'); // borro el cursor
        process_line(input, &history_len);
    }
}


/*-- FUNCIONES AUXILIARES --*/
void read_line(char * buf, uint64_t max) {
    char c;
    uint32_t idx = 0;

    while((c = getchar()) != '\n') {
        if (c == '+') {
            incfont();
        } else if(c == '-') {
            decfont();
        } else if (c == '\b') { // Backspace
			if (idx != 0) {
				idx--;
                if (history_len > 0) {
                    history_len--; 
                }
                putchar('\b'); // borro el cursor
                putchar('\b'); // borro el caracter
                putchar(CURSOR);
			}
		} else if (idx < max) {
            buf[idx++] = c;
            shell_putchar(c, STDOUT);
        }
    }

    buf[idx] = 0;
}


static void add_history(char character, uint64_t fd) {
    if (history_len >= HISTORY_MAX) {
        // Buffer lleno, desplazamos a la izquierda descartando el mas viejo
        for (uint32_t i = 0; i < HISTORY_MAX - 1; i++) {
            history[i] = history[i + 1];
        }
        history_len = HISTORY_MAX - 1; 
    }
    history[history_len].character = character;
    history[history_len].fd = fd;
    history_len++;
}

void shell_putchar(char c, uint64_t fd) {
    char backspace = '\b';
    char cursor = CURSOR;
    add_history(c, fd);
    sys_write(STDOUT, &backspace, 1); // borro el cursor
    sys_write(fd, &c, 1); // escribo el caracter
    sys_write(STDOUT, &cursor, 1); // escribo el cursor
}

void shell_print_string(char *str) {
    if (str == 0) return;
    for (uint32_t i = 0; str[i] != '\0'; i++) {
        shell_putchar(str[i], STDOUT);
    }
}

void shell_print_err(char *str) {
    if (str == 0) return;
    for (uint32_t i = 0; str[i] != '\0'; i++) {
        shell_putchar(str[i], STDERR);
    }
}

void shell_newline(void) {
    shell_putchar('\n', STDOUT);
}


void redraw_history() {
    sys_clear(); 
    if (history_len == 0) {
        return;
    } 
    // guardamos caracteres del mismo fd para llamar menos veces a write
    char buffer[128]; 
    uint64_t current_fd = history[0].fd;
    uint32_t idx = 0;
    
    for (uint32_t i = 0; i < history_len; i++) {
        // si cambia el file descriptor o se lleno el buffer, escribimos
        if (history[i].fd != current_fd || idx >= sizeof(buffer) - 1) {
            if (idx > 0) {
                sys_write(current_fd, buffer, idx);
                idx = 0;
            }
            current_fd = history[i].fd;
        }
        buffer[idx++] = history[i].character;
    }
    
    // Escribir lo que quedo en el buffer
    if (idx > 0) {
        sys_write(current_fd, buffer, idx);
    }
    putchar(CURSOR);

    sys_clear_input_buffer();
}

void incfont()  { 
    sys_increase_fontsize(); 
    redraw_history();
}

void decfont()  { 
    sys_decrease_fontsize(); 
    redraw_history();
}

