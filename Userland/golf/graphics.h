#ifndef GOLF_GRAPHICS_H
#define GOLF_GRAPHICS_H

#include "golf.h"


#define MENU_BKG    0x041803  
#define MENU_CONTENT 0xf4d35e 
#define SCOREBOARD_HEIGHT 32

void init_graphics();
uint32_t get_width();
uint32_t get_height();
void draw_player(Circle *p);
void draw_moving_circle(Circle * circle);
void draw_static_circle (Circle * circle);
int wait_num_players(void);
void level_end_screen(uint8_t winner);
void countdown_screen(uint64_t size);
void final_score_screen(uint8_t two_players, uint16_t score_p1, uint16_t score_p2, uint64_t touches);
void draw_scoreboard(uint8_t two_players);
void update_scoreboard(uint8_t two_players, uint16_t score1, uint16_t score2, uint64_t touches);

#define WAIT_SPACE()   while(getchar_nonblock()!=0x20) 

#endif