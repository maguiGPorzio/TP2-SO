#include "golf.h"
#include "graphics.h"
#include "logic.h"

// -------------------------------------------------------------------------
// PROTOTIPOS DE LAS FUNCIONES
// -------------------------------------------------------------------------

static void game_setup();
static void level_setup();
static void game_loop();
static void init_positions(void);
static void handle_input(Circle* player, const Controls *keys);
static void handle_collisions();
static uint8_t is_ball_in_hole();
static void redraw();

//--------------------------------------------------------------------------
// INICIALIZACIÓN DE VARIABLES Y ESTRUCTURAS
// -------------------------------------------------------------------------

static uint32_t scr_w = 0, scr_h = 0;
static uint8_t two_players = 0; // 1 si es dos jugadores, 0 sino
static int level = 0; 

static uint16_t score_p1 = 0;
static uint16_t score_p2 = 0;
static uint64_t touches = 0;

static Circle player1;
static Circle player2;
static Circle ball;
static Circle hole1;
static Circle hole2; 

void golf_main() {

    game_setup();
    
    // Selección del número de jugadores
    int num = wait_num_players();
    if (num == 0) {
        return;
    }
    two_players = (num == 2);

    level_setup(); 
    while (level < LEVELS && getchar_nonblock() != '\n') {
        game_loop();
    }
    final_score_screen(two_players, score_p1, score_p2, touches);
}


static void game_setup() {
    init_graphics();
    scr_w = get_width();
    scr_h = get_height();
    
    player1.radius = PLAYER_RADIUS;
    player1.color = PLAYER1_COLOR;
    player1.draw = draw_player;
    
    player2.radius = PLAYER_RADIUS;
    player2.color = PLAYER2_COLOR;
    player2.draw = draw_player;
    
    ball.radius = BALL_RADIUS;
    ball.color = BALL_COLOR;
    ball.draw = draw_moving_circle;
    
    hole1.speed = HOLE_SPEED;
    hole1.dir.x = INITIAL_DIR_X_ZERO;
    hole1.dir.y = INITIAL_DIR_Y_ZERO;
    hole1.color = HOLE_COLOR;
    hole1.draw = draw_static_circle;
    
    hole2.speed = HOLE_SPEED;
    hole2.dir.x = INITIAL_DIR_X_ZERO;
    hole2.dir.y = INITIAL_DIR_Y_ZERO;
    hole2.color = HOLE_COLOR;
    hole2.draw = draw_static_circle;
    
    level = 0;
    score_p1 = 0;
    score_p2 = 0;
    touches = 0;
}

static void level_setup() { 
    
    // Reinicializar pelota
    ball.speed = BALL_INITIAL_SPEED;
    ball.dir.x = INITIAL_DIR_X_ZERO;
    ball.dir.y = INITIAL_DIR_Y_ZERO;
    ball.rx = 0.0f;
    ball.ry = 0.0f;
    
    // tamaño segun el nivel
    hole1.radius = HOLE_RADII[level];
    
    
    player1.speed = 0.0;
    player1.rx = 0.0f;
    player1.ry = 0.0f;
    
    if (two_players) {
        hole2.radius = HOLE_RADII[level];
        player2.speed = 0.0;
        player2.rx = 0.0f;
        player2.ry = 0.0f;
        player2.dir.x = PLAYER2_INITIAL_DIR_X;
        player2.dir.y = PLAYER2_INITIAL_DIR_Y;
    }
    player1.dir.x = PLAYER1_INITIAL_DIR_X;
    player1.dir.y = PLAYER1_INITIAL_DIR_Y;
    
    init_positions(); 
    
    hole1.prev.x = hole1.pos.x;
    hole1.prev.y = hole1.pos.y;
    if (two_players) {
        hole2.prev.x = hole2.pos.x;
        hole2.prev.y = hole2.pos.y;
    }
    
    fill_rectangle(0, 0, scr_w, scr_h, BACKGROUND_COLOR_GOLF); // pinto toda la pantalla del color de fondo (verde)
    countdown_screen(6); // 6 es el tamanio del texto
    draw_scoreboard(two_players);
}


static void game_loop() {
    uint64_t ticks = sys_ticks();
    player1.prev.x = player1.pos.x;
    player1.prev.y = player1.pos.y;

    
    if(two_players) {
        player2.prev.x = player2.pos.x;
        player2.prev.y = player2.pos.y;
    }
    
    ball.prev.x = ball.pos.x;
    ball.prev.y = ball.pos.y;

    handle_input(&player1, &P1_KEYS); 
    if(two_players)
        handle_input(&player2, &P2_KEYS);
    
    handle_collisions();
    
    update_ball_motion(&ball, &player1, &player2, two_players);

    uint8_t goal_info = is_ball_in_hole(); // 0 no gano nadie, sino devuelve 1 o 2 segun el ganador
    
    if (goal_info){ 
        uint8_t point_winner_player_num = 0;

        if (two_players) {
            if (goal_info == 1) { 
                score_p1++;
                point_winner_player_num = 1;
                ball.pos.x = hole1.pos.x; 
                ball.pos.y = hole1.pos.y;
            } else { 
                score_p2++;
                point_winner_player_num = 2;
                ball.pos.x = hole2.pos.x; 
                ball.pos.y = hole2.pos.y;
            }
        } else { // un solo jugador, si entro la pelota gano
            score_p1++; 
            point_winner_player_num = 1; 
            ball.pos.x = hole1.pos.x; 
            ball.pos.y = hole1.pos.y;
        }
        
        // redibujar la pelota dentro del hoyo
        ball.draw(&ball); 
        if (goal_info == 1) hole1.draw(&hole1); 
        else if (goal_info == 2 && two_players) hole2.draw(&hole2); 
        fill_circle(ball.pos.x, ball.pos.y, ball.radius, ball.color);
        
        // sonidito de victoria
        sys_beep(800, 221); 
        sys_beep(700, 221);
        level_end_screen(point_winner_player_num);
        level++;
        if (level < LEVELS) {
            level_setup(); 
        }
        return;
    }

    redraw();

    while (ticks == sys_ticks())
        ; // unificamos el tiempo de un gameloop a como minimo un tick

}



static void handle_input(Circle* player, const Controls *key) {
    if (get_key_status(key->forward) || level == 2) { // en el level == 2 va para adelante siempre
        
        player->speed += PLAYER_ACCELERATION;
        if (player->speed > PLAYER_MAX_SPEED) {
            player->speed = PLAYER_MAX_SPEED;
        }
        
        update_position(player); // Mueve el jugador
    } else {
        player->speed = 0.0;
    }
     
    // rotacion
    if (get_key_status(key->right)) {
        rotate_player(player, -ANG);
    }
    if (get_key_status(key->left)) {
        rotate_player(player, ANG);
    }
}

// -------------------------------------------------------------------------
// Inicializacion
// -------------------------------------------------------------------------


void init_positions(){
    player1.pos.x = scr_w / 6;     
    player1.pos.y = scr_h / 2;     // centrar verticalmente


    ball.pos.x = scr_w / 2;        // pelota en el centro de la pantalla
    ball.pos.y = scr_h / 2;

    if (two_players) {
        player2.pos.x = scr_w - scr_w/6; 
        player2.pos.y = scr_h / 2;
        
        // el hoyo donde emboca 1 a la derecha
        hole1.pos.x = scr_w - (scr_w / 10);        
        hole1.pos.y = scr_h / 2; 
        
        // el hoyo donde emboca 2 a la izquierda
        hole2.pos.x = scr_w / 10; 
        hole2.pos.y = scr_h / 2; 
    } else {
        hole1.pos.x = scr_w / 2;        
        hole1.pos.y = SCOREBOARD_HEIGHT + (scr_h - SCOREBOARD_HEIGHT) / 8; 
    }
}


static void handle_collisions() {
    // determinar si cada jugador se está moviendo
    uint8_t player1_moving = get_key_status(P1_KEYS.forward);
    uint8_t player2_moving = two_players ? get_key_status(P2_KEYS.forward) : 0;
    
    if (two_players && collide_circles(&player1, &player2)) {
        handle_player_collision(&player1, &player2, player1_moving, player2_moving);
    }

    if (two_players) {
        if (collide_circles(&player1, &hole1)) resolve_overlap(&hole1, &player1);
        if (collide_circles(&player1, &hole2)) resolve_overlap(&hole2, &player1);
        
        if (collide_circles(&player2, &hole1)) resolve_overlap(&hole1, &player2);
        if (collide_circles(&player2, &hole2)) resolve_overlap(&hole2, &player2);
    } else {
        if (collide_circles(&player1, &hole1)) {
            resolve_overlap(&hole1, &player1);
        }
    }
    
    if (collide_circles(&player1, &ball)) {
        touches++;
        push_ball(&player1, &ball);
        resolve_overlap(&player1, &ball);  // separa jugador de la pelota
        
    }
    
    if (two_players && collide_circles(&player2, &ball)) {
        touches++;
        push_ball(&player2, &ball);
        resolve_overlap(&player2, &ball);  // Separar jugador de pelota
    }

    
}

uint8_t is_ball_in_hole() {
    
    if (is_inside(&ball, &hole1)) {
        return 1;
    }
    if (two_players && is_inside(&ball, &hole2)) {
        return 2;
    }
    
    return 0;
}

static void redraw() {
    player1.draw(&player1);
    hole1.draw(&hole1);
    if (two_players) {
        player2.draw(&player2);
        hole2.draw(&hole2);
    }
    ball.draw(&ball); 
    update_scoreboard(two_players, score_p1, score_p2, touches);
}
