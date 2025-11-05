#include "usrlib.h"

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

int mem(int argc, char *argv[]) {
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

    // Calcular porcentaje de uso
    size_t used = info.used_memory;
    size_t total = info.total_memory;

    if (total == 0) {
        printf("Error: Total memory is 0\n");
        return -1;
    }

    size_t percentUsed = (100 * used) / total;

    // Escala logarítmica simulada con switch
    int barWidth = 13;
    int filled;
    
    switch (percentUsed) {
        case 0 ... 2:
            filled = 1;
            break;
        case 3 ... 4:
            filled = 2;
            break;
        case 5 ... 7:
            filled = 3;
            break;
        case 8 ... 9:
            filled = 4;
            break;
        case 10 ... 19:
            filled = 5;
            break;
        case 20 ... 29:
            filled = 6;
            break;
        case 30 ... 39:
            filled = 7;
            break;
        case 40 ... 49:
            filled = 8;
            break;
        case 50 ... 59:
            filled = 9;
            break;
        case 60 ... 69:
            filled = 10;
            break;
        case 70 ... 79:
            filled = 11;
            break;
        case 80 ... 89:
            filled = 12;
            break;
        case 90 ... 100:
            filled = 13;
            break;
        default:
            filled = 13;
            break;
    }
    
    putchar('[');
    for (int i = 0; i < barWidth; i++) {
        putchar(i < filled ? '#' : '-');
    }
    print("] ");
    print_padded_int((unsigned)percentUsed, 1);
    printf("%%\n");

    printf("Allocated blocks: %u\n", (unsigned)info.allocated_blocks);

    return 0;
}