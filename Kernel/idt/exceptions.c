// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "exceptions.h"
#include "keyboard.h"
#include "videoDriver.h"

static void zero_division();
static void invalid_opcode();
static void excepHandler(char * msg);
extern void returnToUserland(); 
extern void _hlt();
extern void _sti();

static Exception exceptions[]={&zero_division, 0, 0, 0, 0, 0, &invalid_opcode};
static char * message[] = {"Zero Division Exception", "Invalid Opcode Exception"};

void exceptionDispatcher(int exception) {
  	Exception ex = exceptions[exception];
	if(ex != 0){
		ex();
	}
}

static void excepHandler(char * msg){
	vd_print(msg, 0xff0000);
	newline();
	vd_print("Press enter to continue", 0xff0000);
	int c;
	_sti();
	do{
		_hlt();	
	} while((c=get_char_from_buffer()) != '\n');
	vd_clear();
}

static void zero_division() {
	excepHandler(message[0]);
}

static void invalid_opcode(){
	excepHandler(message[1]);
}