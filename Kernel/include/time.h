#ifndef _TIME_H_
#define _TIME_H_

#include <stdint.h>

void timer_handler();
uint64_t ticks_elapsed();
int seconds_elapsed();
void sleep(int seconds);
void get_date(uint8_t * buffer);
void get_time(uint8_t * buffer);

// Import from interrupts.h
extern void _hlt(void);

#endif
