#ifndef GOLF_LOGIC_H
#define GOLF_LOGIC_H

#include "golf.h"

int collide_circles(const Circle *a, const Circle *b);
void push_ball(const Circle *pusher, Circle * ball);
void resolve_overlap(const Circle *fix, Circle *mov);
void update_ball_motion(Circle * ball, Circle * player1, Circle * player2, uint8_t two_players);
uint8_t update_position(Circle *c);
void rotate_player(Circle* player, float angle);
uint8_t clamp_circle_in_screen(Circle *c);
void handle_player_collision(Circle *player1, Circle *player2, uint8_t player1_moving, uint8_t player2_moving);
// devuelve true si a esta dentro de b, 0 sino
uint8_t is_inside(const Circle * a, const Circle * b);
#endif