// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "time.h"
#include "videoDriver.h"
#include "scheduler.h"

extern uint8_t get_hour();
extern uint8_t get_minutes();
extern uint8_t get_seconds();
uint8_t get_day();
uint8_t get_month();    
uint8_t get_year();

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
	buffer[0]=get_day();
	buffer[1]=get_month();
	buffer[2]=get_year();
}

void get_time(uint8_t * buffer){
	buffer[0]=get_hour();
	buffer[1]=get_minutes();
	buffer[2]=get_seconds();
}
