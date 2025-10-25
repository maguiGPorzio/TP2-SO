#include "time.h"
#include "videoDriver.h"
#include "scheduler.h"

extern uint8_t getHour();
extern uint8_t getMinutes();
extern uint8_t getSeconds();
uint8_t getDayOfMonth();
uint8_t getMonth();    
uint8_t getYear();

static uint64_t ticks = 0;

uint64_t timer_handler(uint64_t rsp) {
	ticks++;
	rsp = (uint64_t) schedule((void *) rsp);
	return rsp;
}

uint64_t ticks_elapsed() {
	return ticks;
}

int seconds_elapsed() {
	return ticks / 100;
}

void sleep(int miliseconds) { // normaliza a 10 ms
	unsigned long start_ticks = ticks;
	unsigned long target_ticks = miliseconds / 10; // convertir ms a ticks (100 ticks/seg)
	
	while ((ticks - start_ticks) < target_ticks) {
		_hlt();
	}
}

void get_date(uint8_t * buffer){
	buffer[0]=getDayOfMonth();
	buffer[1]=getMonth();
	buffer[2]=getYear();
}

void get_time(uint8_t * buffer){
	buffer[0]=getHour();
	buffer[1]=getMinutes();
	buffer[2]=getSeconds();
}
