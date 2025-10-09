
// A W D JUGADOR 1 
//J I L JUGADOR 2

#ifndef GOLF_H
#define GOLF_H

#include <stdint.h>
#include "../include/usrlib.h"

// -------------------------------------------------------------------------
// CONSTANTES DEL JUEGO


// #define SLOWDOWN_RATE 350000 // ralentizamos en un jugador para que el juego ande mejor
// Colores
#define BACKGROUND_COLOR_GOLF 0x228B22  // Color verde bosque
#define PLAYER1_COLOR 0xc71de9        // Color magenta
#define PLAYER2_COLOR 0xf18c18        // Color naranja          
#define BALL_COLOR 0xdb1a1a                   
#define HOLE_COLOR 0x000000          // Color negro para el hoyo

// Radios
#define PLAYER_RADIUS 30
#define BALL_RADIUS 10
#define HOLE_RADIUS 20
#define HOLE_RADIUS_L2 15
#define HOLE_RADIUS_L3 11

// Velocidades
#define PLAYER_SPEED 0.15
#define BALL_INITIAL_SPEED 0.0
#define HOLE_SPEED 0.0

// aceleraciones
#define PLAYER_ACCELERATION 0.15  
#define PLAYER_MAX_SPEED 3

// Direcciones iniciales
#define PLAYER1_INITIAL_DIR_X -1.0
#define PLAYER1_INITIAL_DIR_Y 0.0

#define INITIAL_DIR_X_ZERO 0.0
#define INITIAL_DIR_Y_ZERO 0.0

#define PLAYER2_INITIAL_DIR_X 1.0
#define PLAYER2_INITIAL_DIR_Y 0.0

//Ángulo de giro
#define ANG 0.0314f
#define INITIALIZER 0
#define BALL_MAX_SPEED 10
#define BALL_FRICTION_DECELERATION 0.035  // Cantidad de velocidad que se resta por frame (reducida para frenado más lento)
#define BALL_MIN_SPEED_THRESHOLD 0.01     // Umbral mínimo antes de parar completamente (reducido)

typedef struct {
    uint8_t left;
    uint8_t forward;
    uint8_t right;
} Controls;

static const Controls P1_KEYS = { 'a', 'w', 'd' };   // jugador 1
static const Controls P2_KEYS = { 'j', 'i', 'l' };   // jugador 2


// -------------------------------------------------------------------------
// DEFINICIONES Y ESTRUCTURAS
// -------------------------------------------------------------------------

// Estructura para posición (pixeles)
typedef struct {
    int x;
    int y;
} Position;

// Estructura para dirección (versor)
typedef struct {
    double x;
    double y;
} Direction;


// Estructura para círculos genéricos
typedef struct Circle {
    Position prev; // cordenadas anteriores para redibujar
    Position pos; // coordenadas del centro del círculo
    uint32_t radius;
    double speed;  // modulo de velocidad
    Direction dir; // versor de direccion
    uint32_t color;
    void (*draw)(struct Circle*);  // Puntero a funcion de dibujo
    float rx; // acumulador de error fraccional del eje X
    float ry; // acumulador de error fraccional del eje Y
} Circle;

// Bit-mask de bordes tocados
enum {
    EDGE_NONE   = 0,
    EDGE_LEFT   = 1 << 0, // 0001b
    EDGE_RIGHT  = 1 << 1, // 0010b
    EDGE_TOP    = 1 << 2, // 0100b
    EDGE_BOTTOM = 1 << 3  // 1000b
};

#define LEVELS          3

static const uint8_t HOLE_RADII[LEVELS] = {
        HOLE_RADIUS,       
        HOLE_RADIUS_L2,    
        HOLE_RADIUS_L3     
};

/* --------------------------------------------------------- */


// -------------------------------------------------------------------------
// PUNTO DE ENTRADA DEL JUEGO
// -------------------------------------------------------------------------
void golf_main();

#endif