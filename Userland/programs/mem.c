#include "usrlib.h"


// Helper para imprimir número con padding (alineado a la derecha)
static void print_padded_int(unsigned value, int width) {
    // Contar cuántos dígitos tiene el número
    int len = 0;
    unsigned tmp = value;
    
    if (value == 0) {
        len = 1;
    } else {
        while (tmp > 0) {
            len++;
            tmp /= 10;
        }
    }
    
    // Imprimir espacios de padding a la izquierda
    for (int i = 0; i < width - len; i++) {
        putchar(' ');
    }
    
    // Imprimir el número
    printf("%u", value);
}

int mem_main(int argc, char *argv[]) {
    if (argc != 0) {
        printf("mem: Invalid number of arguments.\n");
        return -1;
    }

    MemStatus info = sys_memstatus();

    char *units[] = {"B", "KB", "MB", "GB"};

    size_t values[] = {info.total_memory, info.used_memory, info.free_memory};
    const char *labels[] = {"Total", "Used", "Free"};

    // Mostrar información de memoria
    for (int i = 0; i < 3; i++) {
        size_t val = values[i];
        int unitIndex = 0;
        double convertedVal = (double)val;

        while (convertedVal >= 1024.0 && unitIndex < 3) {
            convertedVal /= 1024.0;
            unitIndex++;
        }

        int rounded = (int)(convertedVal + 0.5);
        
        // Padding manual para las etiquetas
        printf("%s: ", labels[i]);
        
        // Número en bytes con padding (alineado a 8 caracteres)
        print_padded_int((unsigned)val, 8);
        
        // Valor convertido
        printf(" (%u %s)\n", (unsigned)rounded, units[unitIndex]);
    }

    printf("Allocated blocks: %u\n", (unsigned)info.allocated_blocks);

    return OK;
}