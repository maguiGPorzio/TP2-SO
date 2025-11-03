#include "usrlib.h"

static uint64_t print_udecimal(uint64_t value);
static uint64_t print_decimal(int64_t value);
static uint64_t print_hex(uint64_t value, uint8_t uppercase);
static uint64_t print_pointer(uint64_t ptr);
static uint64_t print_oct(uint64_t value);
static uint64_t print_bin(uint64_t value);
static uint64_t print_float(double num);

uint64_t fprint(uint64_t fd, char * str) {
    if (str == 0)
        return 0;
    return sys_write(fd, str, strlen(str));
}

uint64_t print(char *str) {
    return fprint(STDOUT, str);   
}

uint64_t print_err(char * str){
    return fprint(STDERR, str);
}



// Retorna 1 si se pudo escribir, 0 si no
uint64_t putchar(char c) {
    return sys_write(STDOUT, &c, 1);
}

char getchar(){
    char c;
    sys_read(STDIN, &c, 1);
    return c;
}

uint64_t printf_aux(const char *fmt, const uint64_t *intArgs, const uint64_t *stackPtr, const double *floatArgs) {
    uint64_t chars_written = 0;
    uint64_t int_idx = 0;
    uint64_t stk_idx = 0;
    uint64_t float_idx = 0;

    if(!fmt) return 0; 
    
    while (*fmt) {
        if (*fmt != '%') {        
            putchar(*fmt++);
            chars_written++;
        } else{
            fmt++;                
            if (*fmt == '%') {    
                putchar('%');
                chars_written++;
                fmt++;
            } else{
                uint64_t uint_arg;
                int64_t int_arg;
                double float_arg;
                if (*fmt == 'd' || *fmt == 'i'){ 
                    if (int_idx < NUM_INT_REGS){
                        int_arg = *(int64_t*)&intArgs[int_idx++];   
                    } else {
                        int_arg = *(int64_t*)&stackPtr[stk_idx++];
                    }   
                } else if (*fmt == 'u' || *fmt == 'x' || *fmt == 'X' || *fmt == 'o' || *fmt == 'b' || *fmt == 'p' || *fmt == 's' || *fmt == 'c') { // Me guardo el argumento como unsigned int
                    if (int_idx < NUM_INT_REGS){
                        uint_arg=intArgs[int_idx++];
                    } else {
                        uint_arg = stackPtr[stk_idx++];
                    }
                } else { 
                    if (float_idx < NUM_SSE_REGS){
                        float_arg=floatArgs[float_idx++];
                    } else {
                        float_arg = 0.0;  
                    }
                } 
                
                switch (*fmt) {         
                    case 'c': 
                        chars_written += putchar((char)uint_arg);
                        break;
                    case 's':  
                        chars_written += print((char*)uint_arg);      
                        break;
                    case 'u':  
                        chars_written += print_udecimal(uint_arg);
                        break;
                    case 'd':  
                    case 'i':  
                        chars_written += print_decimal(int_arg);
                        break;
                    case 'x':  
                        chars_written += print_hex(uint_arg, 0);
                        break;
                    case 'X':  
                        chars_written += print_hex(uint_arg, 1);
                        break;
                    case 'o': 
                        chars_written += print_oct(uint_arg);
                        break;
                    case 'b':  
                        chars_written += print_bin(uint_arg);
                        break;
                    case 'p':  
                        chars_written += print_pointer(uint_arg); 
                        break;
                    case 'f':  
                        chars_written += print_float(float_arg);  
                        break;  
                    case 'F':  
                     
                        break;
                    case 'e': 

                        break;
                    case 'E':  

                        break;
                    case 'g':  

                        break;
                    case 'a':                      
                        break;
                    default:   
                        // EXCEPCIÓN???
                        break;
 
                }
                fmt++;
            }
        }
    }
    return chars_written;
}

static uint64_t print_hex(uint64_t value, uint8_t uppercase) {
    char buffer[HEX_BUFFER_SIZE]; // Suficiente para un entero de 64 bits
    uint64_t len = num_to_str(value, buffer, 16);
    // Convertir a mayúsculas si es necesario
    if (uppercase) {
        for (uint64_t i = 0; i < len; i++) {
            if (buffer[i] >= 'a' && buffer[i] <= 'f') {
                buffer[i] -= ('a' - 'A');
            }
        }
    }
    return sys_write(STDOUT, buffer, len);
}

// Imprime el puntero en hexadecimal
static uint64_t print_pointer(uint64_t ptr) {
    uint64_t prefix = print("0x");
    return print_hex(ptr, 0) + prefix; 
}

static uint64_t print_oct(uint64_t value) {
    char buffer[OCTAL_BUFFER_SIZE]; // Suficiente para un entero de 64 bits en base 8
    uint64_t len = num_to_str(value, buffer, 8);
    return sys_write(STDOUT, buffer, len);
}

static uint64_t print_bin(uint64_t value) {
    char buffer[BINARY_BUFFER_SIZE]; // Suficiente para un entero de 64 bits en base 2
    uint64_t len = num_to_str(value, buffer, 2);
    return sys_write(STDOUT, buffer, len);
}



static uint64_t print_float(double num) {
    uint64_t count = 0;

    if (num < 0) {
        count += putchar('-');
        num = -num;
    }

    // Parte entera
    uint64_t int_part = (uint64_t) num;
    count += print_udecimal(int_part);

    count += putchar('.');

    // Parte decimal
    double frac_part = num - int_part;
    for (int i = 0; i < FLOAT_PRECISION; i++) {
        frac_part *= 10;
    }

    uint64_t frac_int = (uint64_t)(frac_part + 0.5); // redondeo
    // Asegurarse de imprimir ceros a la izquierda si es necesario
    uint64_t divisor = 1;
    for (int i = 1; i < FLOAT_PRECISION; i++) divisor *= 10;
    while (frac_int < divisor) {
        count += putchar('0');
        divisor /= 10;
    }

    count += print_udecimal(frac_int);
    return count;
}

uint64_t get_key_status(char key) {
    return sys_key_status(key);
}

float inv_sqrt(float number){
    const float threehalfs = 1.5F;
    float x2 = number * 0.5F;

    union { float f; uint32_t i; } conv = { .f = number };
    conv.i  = 0x5f3759df - (conv.i >> 1);  
    float y = conv.f;

    y = y * (threehalfs - (x2 * y * y));
    return y;             
}

uint64_t scanf_aux(const char *fmt, uint64_t regPtr[], uint64_t stkPtr[]) {
    uint64_t items_read = 0;
    uint64_t regs_idx = 0;
    uint64_t stk_idx = 0;

    if(!fmt){
        return 0; 
    }
    
    while (*fmt) {
        if (*fmt != '%') { 
            fmt++;
        } else {
            fmt++;
            switch (*fmt) {
                case 'c':  
                    {
                        char *ptr;
                        if (regs_idx < NUM_INT_REGS) {
                            ptr = (char*)regPtr[regs_idx++];
                        } else {
                            ptr = (char*)stkPtr[stk_idx++];
                        }
                        *ptr = getchar();
                        items_read++;
                    }
                    break;
                case 's':  
                    {
                        char *str_ptr;
                        if (regs_idx < NUM_INT_REGS) {
                            str_ptr = (char*)regPtr[regs_idx++];
                        } else {
                            str_ptr = (char*)stkPtr[stk_idx++];
                        }
                        
                        char c;
                        int i = 0;
                    
                        while ((c = getchar()) != '\n') {
                            str_ptr[i++] = c;
                        }
                        str_ptr[i] = '\0';
                        items_read++;
                    }
                    break;
                case 'd':  
                    {
                        int *ptr;
                        if (regs_idx < NUM_INT_REGS) {
                            ptr = (int*)regPtr[regs_idx++];
                        } else {
                            ptr = (int*)stkPtr[stk_idx++];
                        }
                        
                        uint64_t entero = 0;
                        char r = getchar();
                        while (r != '\n') {
                            entero *= 10;
                            entero += (r - '0');
                            r = getchar();
                        }
                        *ptr = (int)entero;
                        items_read++;
                    }
                    break;
                default:   
                    break;
                }
                fmt++;
            }
    }
    return items_read;
}

// Imprime el unsigned uint64_t en base 10 y devuelve la cantidad de caracteres escritos
static uint64_t print_udecimal(uint64_t value) {
    char buffer[DECIMAL_BUFFER_SIZE]; // Suficiente para un entero de 64 bits
    uint64_t len = num_to_str(value, buffer, 10);
    return sys_write(STDOUT, buffer, len);
}

// Imprime el signed uint64_t en base 10 y devuelve la cantidad de caracteres escritos
static uint64_t print_decimal(int64_t value) {
    if (value < 0) {
        putchar('-');
        value = -value; // Convertir a positivo para imprimir
        return print_udecimal((uint64_t)value) + 1; // +1 por el signo negativo
    }
    return print_udecimal((uint64_t)value);
}