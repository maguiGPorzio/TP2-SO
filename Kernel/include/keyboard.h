#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define LEFT_SHIFT  0x2A
#define RIGHT_SHIFT 0x36
#define CAPS_LOCK 0x3A
#define LEFT_CONTROL 0x1D
#define LEFT_ARROW  0x4B
#define RIGHT_ARROW  0x4D
#define UP_ARROW  0x48
#define DOWN_ARROW  0x50
#define BREAKCODE_OFFSET 0x80
#define AMOUNT_REGISTERS 20
#define BUFFER_LENGTH 256
#define LETTERS 26

extern char getPressedKey();
extern uint64_t reg_array[]; 

void init_keyboard_sem();
void printRegisters();
void clear_buffer();
uint8_t getCharFromBuffer();
uint64_t read_keyboard_buffer(char * buff_copy, uint64_t count);
void handlePressedKey();
void storeSnapshot();
uint64_t copyRegisters(char * copy);
uint32_t uint64ToRegisterFormat(uint64_t value, char *dest);
uint8_t is_pressed_key(char c);
void writeStringToBuffer(const char *str);

#endif


