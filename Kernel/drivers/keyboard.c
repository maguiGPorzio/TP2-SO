#include "keyboard.h"
#include "lib.h"
#include "naiveConsole.h"
#include "videoDriver.h"

static int shift = 0 ;
static int caps_lock = 0;
static int copied_registers=0;

uint16_t buffer_start = 0; // índice del buffer del próximo carácter a leer 
uint16_t buffer_end = 0; // índice del buffer donde se va a escribir el próximo caracter recibido en el teclado
uint16_t buffer_current_size = 0; // cantidad de caracteres en el buffer actual (listos para ser leídos)

static uint8_t buffer[BUFFER_LENGTH];
static char reg_buff[800]; // ACA FIJARNOS QUÉ TAMAÑO NOS CONVIENE

static void writeBuffer(unsigned char c);

static const char lowerKeys[] = {
      0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',
   '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  '[', ']',
   '\n',    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
      0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',    0, '*',
      0,  ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,
      0,    0,   38,   0, '-',   37,   0,   39, '+',   0,   40,   0,    0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,  0,    0,   0,   0,    0,   0,
};

static const char upperKeys[] = {
      0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
   '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
   '\n',    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
      0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',
      0, ' ',    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,    0,   0, '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,
      0,   0,    0,   0,   0,   0
};

static const char * scancodeToAscii[] = {lowerKeys, upperKeys};

static uint8_t pressedKeys[LETTERS] = {0};

// Static porque no queremos que se pueda acceder desde otro archivo
static void writeBuffer(unsigned char c) {
    buffer[buffer_end] = c;
    buffer_end = (buffer_end + 1) % BUFFER_LENGTH; // si hay buffer overflow, se pisa lo del principio (jodete usuario)
    buffer_current_size = (buffer_current_size + 1) % BUFFER_LENGTH;
}

void clear_buffer() {
  buffer_end = buffer_start = buffer_current_size = 0;
}

uint8_t getCharFromBuffer() {
	if(buffer_current_size == 0){
		return -1;
	}
	--buffer_current_size;
	uint8_t result = buffer[buffer_start];
	buffer_start = (buffer_start + 1) % BUFFER_LENGTH;
	return result;
}

// copia en el buff lo que hay en el buffer de teclado hasta count y va vaciando el buffer de teclado
uint64_t read_keyboard_buffer (char * buff_copy, uint64_t count) {
    int i;
    for (i=0; i < count && i < buffer_current_size; i++) {
        buff_copy[i] = getCharFromBuffer();
    }
    return i;
}


void handlePressedKey() {
    unsigned char scancode = getPressedKey(); // this function retrieves the pressed key's scancode

    if (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT){ 
        shift = 1;
    } else if (scancode == LEFT_SHIFT + BREAKCODE_OFFSET || scancode == RIGHT_SHIFT + BREAKCODE_OFFSET) { 
        shift = 0;
    } else if (scancode == LEFT_CONTROL) {
        copied_registers = 1;
        storeSnapshot();
        return; 
    } else if (scancode == CAPS_LOCK) {
        caps_lock = (caps_lock+1)%2;
    } else if (scancode == 0){
        return;
    }else if (scancode > BREAKCODE_OFFSET){ // se soltó una tecla o es un caracter no imprimible
        char raw = lowerKeys[scancode - BREAKCODE_OFFSET]; 
        if (raw >= 'a' && raw <= 'z') {
            pressedKeys[raw-'a'] = 0; // marcamos la tecla como no presionada
        }
        return;
    } else if (scancode == UP_ARROW || scancode == DOWN_ARROW || scancode == LEFT_ARROW || scancode == RIGHT_ARROW || scancode==0){  
        return;
    } else {
        int index;                      
        char raw = lowerKeys[scancode]; 
        int isLetter = (raw >= 'a' && raw <= 'z');  

        if (isLetter) {
            index = shift ^ caps_lock;
            pressedKeys[raw-'a'] = 1;
        } else {
            index = shift;                      
        }

        writeBuffer(scancodeToAscii[index][scancode]);
		// vdPutChar(scancodeToAscii[index][scancode], 0xffffff);
    }

    return;
}

void writeStringToBuffer(const char *str) {
    while (*str) {
        writeBuffer((unsigned char)*str);
        str++;
    }
}

uint8_t is_pressed_key(char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) { 
        c = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; // Convertir a minúscula si es mayúscula
        return pressedKeys[c-'a']; // Devuelve 1 si la tecla está presionada, 0 si no
    }
    return 0; // Si el char es inválido, retornamos 0
}

void storeSnapshot(){
  char * reg_labels[] = {"RAX:    0x", "RBX:    0x", "RCX:    0x", "RDI:    0x", "RBP:    0x", "RDI:    0x", "RSI:    0x",  
    "R8:     0x", "R9:     0x", "R10:    0x", "R11:    0x", "R12:    0x", "R13:    0x", "R14:    0x", "R15:    0x","RIP:    0x","CS:     0x","RFLAGS: 0x","RSP:    0x", "SS:     0x", 0};
  uint32_t j = 0; //índice de reg_buff

  for(int i=0 ; reg_labels[i] ; ++i){
    //Agregamos el string al buffer
    for(int m=0; reg_labels[i][m]; ++m){
      reg_buff[j++] = reg_labels[i][m];
    }

    //Agregamos el nro al buffer. Quiero que todos me queden con 16 dígitos hexadecimales
    j += uint64ToRegisterFormat(reg_array[i], reg_buff + j);
    reg_buff[j++] = '\n';
  }
  reg_buff[j] = 0;
  // printRegisters();
}

// devuelve la cantidad de caracteres escritos
uint32_t uint64ToRegisterFormat(uint64_t value, char *dest) {
    int64_t zeros_to_pad = 16;
    uint64_t aux = value;

    // Calcular cuántos ceros hay que agregar
    while (aux) {
        aux >>= 4;
        zeros_to_pad--;
    }

    uint32_t j = 0;
    // Agregar los ceros necesarios
    for (int i = 0; i < zeros_to_pad; i++) {
        dest[j++] = '0';
    }

    // Escribir la parte significativa si value ≠ 0
    if (value) {
        j += uintToBase(value, dest + j, 16);
    }

    dest[j] = 0; // null-terminador por si hace falta usarlo como string
    return j;    // devuelve la cantidad de caracteres escritos
}


//devuelve 1 si ya se llamo a ctrl s y 0 si todavia no se llamo
//dejamos en copy un string con el nombre del registro y su valor (cada registro separado por un n)
uint64_t copyRegisters(char * copy){
    if (!copied_registers){
        return 0;
    }
    int i;
    for (i=0 ; reg_buff[i] ; i++){
        copy[i]=reg_buff[i];
    }
    copy[i] = 0;
    return 1;
}

// void printRegisters() {
//     ncClear();
//     for (int i = 0; reg_buff[i] != 0; i++) {
//         ncPrintChar(reg_buff[i]);
//     }
// }