#include "logic.h"
#include "graphics.h"

// Devuelve 1 si a y b chocaron
int collide_circles(const Circle *a, const Circle *b){
    int dx = a->pos.x - b->pos.x; 
    int dy = a->pos.y - b->pos.y; 
    int radSum = (int)a->radius + (int)b->radius; 

    return (dx*dx + dy*dy) <= (radSum * radSum); 
}

void push_ball(const Circle *pusher, Circle *ball) {
    // vector normal (desde pusher hacia ball)
    float nx = (float)ball->pos.x - (float)pusher->pos.x;
    float ny = (float)ball->pos.y - (float)pusher->pos.y;
    float n2 = nx*nx + ny*ny;

    if (n2 == 0.0f) {               
        nx = pusher->dir.x;
        ny = pusher->dir.y;
        n2 = nx*nx + ny*ny;
    }
    float invN = inv_sqrt(n2);      
    nx *= invN;
    ny *= invN;                     

    // velocidades en el sistema de coordenadas
    float v1x = pusher->dir.x * pusher->speed;  // velocidad del pusher
    float v1y = pusher->dir.y * pusher->speed;
    float v2x = ball->dir.x * ball->speed;      // velocidad de la pelota
    float v2y = ball->dir.y * ball->speed;

    // velocidad relativa proyectada en la normal
    float rvx = v1x - v2x;
    float rvy = v1y - v2y;
    float velN = rvx*nx + rvy*ny;

    // verificar si el pusher está en movimiento
    if (pusher->speed <= 0.01f) {
        // pusher quieto - rebote completamente elástico
        float v_ball_normal = v2x*nx + v2y*ny; // velocidad de la pelota en la normal
        
        // reflejar solo la componente normal sin pérdida de energía
        float vb_x = v2x - 2.0f * v_ball_normal * nx;
        float vb_y = v2y - 2.0f * v_ball_normal * ny;
        
        float vb2 = vb_x*vb_x + vb_y*vb_y;
        if (vb2 > 0.0f) {
            float speed = inv_sqrt(1.0f / vb2);
            float invV = inv_sqrt(vb2);
            ball->dir.x = vb_x * invV;
            ball->dir.y = vb_y * invV;
            ball->speed = speed;
        }
    } else if (velN != 0.0f) {
        // pusher en movimiento - colisión elástica estándar (funciona para acercándose Y alejándose)
        float mass_ratio = 2.5;
        float vb_x = v2x + mass_ratio * velN * nx;
        float vb_y = v2y + mass_ratio * velN * ny;

        // calcular la nueva velocidad y dirección de la pelota
        float vb2 = vb_x*vb_x + vb_y*vb_y;
        if (vb2 > 0.0f) {
            float speed = inv_sqrt(1.0f / vb2);  // nueva velocidad
            float invV = inv_sqrt(vb2);
            ball->dir.x = vb_x * invV;
            ball->dir.y = vb_y * invV;
            ball->speed = speed;
            
            // aplicar límite máximo de velocidad
            if (ball->speed > BALL_MAX_SPEED) {
                ball->speed = BALL_MAX_SPEED;
            }
        } else {
            // si no hay velocidad resultante, empujar en dirección normal
            ball->dir.x = nx;
            ball->dir.y = ny;
            ball->speed = pusher->speed * mass_ratio;
            if (ball->speed > BALL_MAX_SPEED) {
                ball->speed = BALL_MAX_SPEED;
            }
        }
    }
    // Si velN == 0 (movimiento perpendicular), no hacer nada
}

// mueve a mov para que no este mas overlapeado con el circulo fix
void resolve_overlap(const Circle *fix, Circle *mov){
    float dx = (float)mov->pos.x - (float)fix->pos.x;
    float dy = (float)mov->pos.y - (float)fix->pos.y;
    float dist2 = dx*dx + dy*dy;

    if (dist2 == 0.0f) {
        dx = 1.0f;   
        dy = 0.0f;
        dist2 = 1.0f;
    }

    float dist   = inv_sqrt(dist2);      
    float radSum = (float)fix->radius + (float)mov->radius;
    float separation = 2.0f;  // separo 2px de mas porque sino a veces quedaban colisionando           
    float overlap = radSum + separation - (1.0f / dist);

    float pushX = dx * dist * overlap;
    float pushY = dy * dist * overlap;

    // redondea
    mov->pos.x += (int)(pushX + (pushX > 0 ? 0.5f : -0.5f));
    mov->pos.y += (int)(pushY + (pushY > 0 ? 0.5f : -0.5f));


    // actualizamos el resto porque sino lo puede volver a mover para adentro
    mov->rx = mov->ry = 0.0f;
    clamp_circle_in_screen(mov);
}

void update_ball_motion(Circle * ball, Circle * player1, Circle * player2, uint8_t two_players) {
    if (ball->speed > 0.0){
        uint8_t hit = update_position(ball); // hit almacena si chocó con el borde de la pantalla y debe rebotar
        if (hit & (EDGE_LEFT | EDGE_RIGHT)) { // Si choco con los bordes de los costados
            ball->dir.x = -ball->dir.x; // Invierto la dirección en x
            ball->rx = 0.0f; // Reseteo el resto
        }
        if (hit & (EDGE_TOP | EDGE_BOTTOM)) { // Si choco con el borde superior o inferior
            ball->dir.y = -ball->dir.y; // Invierto la dirección en y
            ball->ry = 0.0f; // Reseteo el resto
        }

        // freno
        if (ball->speed > BALL_FRICTION_DECELERATION) {
            ball->speed -= BALL_FRICTION_DECELERATION;
        } else if (ball->speed > BALL_MIN_SPEED_THRESHOLD) {
            // velociad chica pero menor que el threshold
            ball->speed -= ball->speed * 0.1; 
        } else {
            // si esta por abajo del threshold la detenemos
            ball->speed = 0.0;
        }

    }

    clamp_circle_in_screen(ball);
}

uint8_t update_position(Circle *c){

    c->rx += c->speed * c->dir.x; 
    c->ry += c->speed * c->dir.y; 

    int dx = (int)c->rx; 
    int dy = (int)c->ry;


    c->rx -= dx; 
    c->ry -= dy; 

    c->pos.x += dx;
    c->pos.y += dy;

    return clamp_circle_in_screen(c);
}


void rotate_player(Circle* player, float angle) {
    float nx = player->dir.x + angle * player->dir.y;
    float ny = player->dir.y - angle * player->dir.x;
    float inv = inv_sqrt(nx*nx + ny*ny);
    player->dir.x = nx * inv;
    player->dir.y = ny * inv;
}

// simetrica para ambos jugadores
void handle_player_collision(Circle *player1, Circle *player2, uint8_t player1_moving, uint8_t player2_moving) {
    
    // calcular el vector entre centros (desde player1 hacia player2)
    float dx = (float)player2->pos.x - (float)player1->pos.x;
    float dy = (float)player2->pos.y - (float)player1->pos.y;
    float dist2 = dx*dx + dy*dy;
    
    if (dist2 == 0.0f) {
        // Centros exactamente iguales, usar dirección de algún jugador que se mueva
        if (player1_moving) {
            dx = player1->dir.x;
            dy = player1->dir.y;
        } else if (player2_moving) {
            dx = player2->dir.x;
            dy = player2->dir.y;
        } else {
            dx = 1.0f;
            dy = 0.0f;
        }
        dist2 = dx*dx + dy*dy;
    }
    
    float inv_dist = inv_sqrt(dist2);
    float currentDist = 1.0f / inv_dist; // Distancia actual entre centros
    dx *= inv_dist; // Normalizar - vector unitario desde player1 hacia player2
    dy *= inv_dist;
    
    float radSum = (float)(player1->radius + player2->radius);
    float minSeparation = radSum + 5.0f; // aumentamos la separacion minima porque a veces se quedaban trabados
    
    if (currentDist < minSeparation) {
        float overlap = minSeparation - currentDist;
        float pushDistance = overlap * 0.6f; // empuje fuerte para separación
        
        // empujar cada jugador en direcciones opuestas
        player1->pos.x -= (int)(dx * pushDistance + 0.5f);
        player1->pos.y -= (int)(dy * pushDistance + 0.5f);
        player2->pos.x += (int)(dx * pushDistance + 0.5f);
        player2->pos.y += (int)(dy * pushDistance + 0.5f);
        
        clamp_circle_in_screen(player1);
        clamp_circle_in_screen(player2);
        
        // resetear restos
        player1->rx = player1->ry = 0.0f;
        player2->rx = player2->ry = 0.0f;
        
        // reducir velocidad para evitar re-colisiones inmediatas
        player1->speed *= 0.8f;
        player2->speed *= 0.8f;
    }
    
}

uint8_t is_inside(const Circle * a, const Circle * b) {
    int dx = b->pos.x - a->pos.x;
    int dy = b->pos.y - a->pos.y;

    // Check if currently inside
    if (b->radius * b->radius > dx*dx + dy*dy) {
        return 1;
    }

    // Check if the line segment from prev to current position intersected the circle
    float px = (float)a->prev.x;
    float py = (float)a->prev.y;
    float cx = (float)a->pos.x;
    float cy = (float)a->pos.y;
    float bx = (float)b->pos.x;
    float by = (float)b->pos.y;
    float r = (float)b->radius;

    // Vector from prev to current
    float vx = cx - px;
    float vy = cy - py;
    
    // Vector from prev to circle center
    float wx = bx - px;
    float wy = by - py;
    
    float v_dot_v = vx*vx + vy*vy;
    
    // If no movement, no intersection
    if (v_dot_v == 0.0f) {
        return 0;
    }
    
    // Project circle center onto the line
    float t = (wx*vx + wy*vy) / v_dot_v;
    
    // Clamp t to [0,1] for line segment
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Closest point on line segment to circle center
    float closest_x = px + t * vx;
    float closest_y = py + t * vy;
    
    // Distance from circle center to closest point
    float dist_x = bx - closest_x;
    float dist_y = by - closest_y;
    float dist_sq = dist_x*dist_x + dist_y*dist_y;
    
    // Check if distance is less than radius (intersection)
    return (dist_sq <= r*r) ? 1 : 0;
}

uint8_t clamp_circle_in_screen(Circle *c){
    uint8_t hit = EDGE_NONE;
    int r = (int)c->radius; // para que ninguna parte del circulo atraviese el borde
    
    uint32_t scr_h = get_height();
    uint32_t scr_w = get_width();
    
    if (c->pos.x <  r){ // evita que se meta por la izquierda 
        c->pos.x =  r; 
        hit |= EDGE_LEFT;
    } else if (c->pos.x > (int)scr_w-1-r) { // evita que se meta por derecha 
        c->pos.x =  scr_w-1-r;
        hit |= EDGE_RIGHT;
    }

    
    if (c->pos.y < r+SCOREBOARD_HEIGHT) {  // evita que se meta por arriba
        c->pos.y =  r+SCOREBOARD_HEIGHT;
        hit |= EDGE_TOP;
    } else if (c->pos.y > (int)scr_h-1-r){ // evita que se meta por abajo 
        c->pos.y =  scr_h-1-r;
        hit |= EDGE_BOTTOM;
    }

    return hit; 
}