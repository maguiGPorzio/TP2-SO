#include "golf.h"
#include "logic.h"
#include "graphics.h"



uint32_t width, height;

void init_graphics() {
    sys_screen_size(&width, &height);
}

uint32_t get_width() {
    return width;
}
uint32_t get_height() {
    return height;
}

void draw_player(Circle *p){
    fill_circle(p->prev.x, p->prev.y, p->radius, BACKGROUND_COLOR_GOLF);
    // cuerpo
    fill_circle(p->pos.x, p->pos.y, p->radius, p->color);

    float dx = p->dir.x;
    float dy = p->dir.y;
    float len2 = dx*dx + dy*dy;          
    if (len2 == 0.0f) {                  
        dx = 1.0f; dy = 0.0f; len2 = 1.0f;
    }
    float invLen = inv_sqrt(len2);       
    float nx = dx * invLen;              
    float ny = dy * invLen;

    float px = -ny;                      
    float py =  nx;

    // flecha
    int arrowLength = p->radius * 2 / 3;    
    int arrowHeadSize = p->radius / 4;      
    int baseX = (int)(p->pos.x - nx * arrowLength / 2);
    int baseY = (int)(p->pos.y - ny * arrowLength / 2);
    int tipX = (int)(p->pos.x + nx * arrowLength / 2);
    int tipY = (int)(p->pos.y + ny * arrowLength / 2);
    
    draw_line(baseX, baseY, tipX, tipY, 0x000000);

    // puntas de la flecha
    int wing1X = (int)(tipX - nx * arrowHeadSize - px * arrowHeadSize / 2);
    int wing1Y = (int)(tipY - ny * arrowHeadSize - py * arrowHeadSize / 2);
    int wing2X = (int)(tipX - nx * arrowHeadSize + px * arrowHeadSize / 2);
    int wing2Y = (int)(tipY - ny * arrowHeadSize + py * arrowHeadSize / 2);
    
    draw_line(tipX, tipY, wing1X, wing1Y, 0x000000);
    draw_line(tipX, tipY, wing2X, wing2Y, 0x000000);

    // ojos
    int eyeDist   = p->radius / 2;
    int pupilOff  = p->radius / 4;
    int eyeRad    = 3;

    int lx = (int)(p->pos.x +  px * eyeDist);
    int ly = (int)(p->pos.y +  py * eyeDist);
    int rx = (int)(p->pos.x -  px * eyeDist);
    int ry = (int)(p->pos.y -  py * eyeDist);

    int pupX = (int)(nx * pupilOff);
    int pupY = (int)(ny * pupilOff);

    fill_circle(lx + pupX, ly + pupY, eyeRad, 0x000000);
    fill_circle(rx + pupX, ry + pupY, eyeRad, 0x000000);
}

void draw_moving_circle(Circle * circle) {
    fill_circle(circle->prev.x, circle->prev.y, circle->radius, BACKGROUND_COLOR_GOLF);
    fill_circle(circle->pos.x, circle->pos.y, circle->radius, circle->color);
}

void draw_static_circle (Circle * circle) {
    fill_circle(circle->pos.x, circle->pos.y, circle->radius, circle->color);
}

// PANTALLAS
int wait_num_players(void) {
    int num_players;

    fill_rectangle(0, 0, width, height, MENU_BKG);
    
    int title_y = height / 3;
    int instruction_y = height / 2;
    
    draw_string("GOLF GAME", width/2 - 144, title_y, 4, MENU_CONTENT);
    draw_string("Select number of players:", width/2 - 200, instruction_y, 2, MENU_CONTENT);
    draw_string("Press 1 for Single Player (WAD)", width/2 - (30 * 8 * 2 / 2), instruction_y + 40, 2, MENU_CONTENT);
    draw_string("Press 2 for Two Players (WAD + IJL)", width/2 - (33 * 8 * 2 / 2), instruction_y + 70, 2, MENU_CONTENT);
    draw_string("Press Enter to quit the game at any time", width/2 - 168, instruction_y + 120, 1, 0x888888); // Gray color for less emphasis

    do {
        num_players = getchar();  // bloq: NO sale hasta que se presione una tecla 
    } while (num_players != '1' && num_players != '2' && num_players != '\n'); // ignora cualquier tecla que no sea 1 o 2
    
    if (num_players == '\n') {
        return 0;
    }

    return num_players - '0'; // devuelve 1 o 2 como enteros
}

void level_end_screen(uint8_t winner) {

    fill_rectangle(0, 0, width, height, MENU_BKG);
    
    int title_y = height / 3;
    int subtitle_y = height / 2;
    int instruction_y = height / 2 + 60;
    
    draw_string("GOAL!", width/2 - 80, title_y, 4, MENU_CONTENT);
    
    if (winner == 1) {
        draw_string("Player 1 scored!", width/2 - 192, subtitle_y, 3, MENU_CONTENT);
    } else {
        draw_string("Player 2 scored!", width/2 - 192, subtitle_y, 3, MENU_CONTENT);
    }

    draw_string("Press SPACE to continue", width/2 - 184, instruction_y, 2, MENU_CONTENT);
    
    WAIT_SPACE();
}



static void build_score_string(char* dest, const char* player_name, uint16_t score) {
    const char* prefix = player_name;
    int i = 0;
    while (*prefix) {
        dest[i++] = *prefix++;
    }
    dest[i++] = ':';
    dest[i++] = ' ';
    
    char score_buffer[16];
    uint64_t score_len = num_to_str(score, score_buffer, 10);
    for (int j = 0; j < score_len; j++) {
        dest[i++] = score_buffer[j];
    }
    dest[i] = '\0';
}

void final_score_screen(uint8_t two_players, uint16_t score_p1, uint16_t score_p2, uint64_t touches) {

    
    fill_rectangle(0, 0, width, height, MENU_BKG);
    
    int title_y = height / 4;
    int content_start_y = height / 2 - 60;
    int instruction_y = height / 2 + 120;
    
    if (two_players) {
        draw_string("FINAL SCORE", width/2 - 176, title_y, 4, MENU_CONTENT);
        
        // Player 1 score
        char score_text[32];
        build_score_string(score_text, "Player 1", score_p1);
        int p1_text_width = strlen(score_text) * 8 * 3;
        draw_string(score_text, width/2 - p1_text_width/2, content_start_y, 3, MENU_CONTENT);
        
        build_score_string(score_text, "Player 2", score_p2);
        int p2_text_width = strlen(score_text) * 8 * 3;
        draw_string(score_text, width/2 - p2_text_width/2, content_start_y + 50, 3, MENU_CONTENT);
        
        int winner_y = content_start_y + 100;
        if (score_p1 > score_p2) {
            draw_string("Player 1 Wins!", width/2 - 168, winner_y, 3, 0x00FF00); // Gerde para el ganadorreen for winner
        } else if (score_p2 > score_p1) {
            draw_string("Player 2 Wins!", width/2 - 168, winner_y, 3, 0x00FF00); // Verde para el ganador
        } else {
            draw_string("It's a Tie!", width/2 - 132, winner_y, 3, 0xFFFF00); // Amarillo para empate
        }
        
        // toques
        char touches_text[32];
        build_score_string(touches_text, "Total Touches", (uint16_t)touches);
        int touches_text_width = strlen(touches_text) * 8 * 2;
        draw_string(touches_text, width/2 - touches_text_width/2, winner_y + 60, 2, MENU_CONTENT);
        
        instruction_y = winner_y + 100;
    } else {
        // solo mostramos toques
        draw_string("GAME COMPLETE", width/2 - 208, title_y, 4, MENU_CONTENT);
        
        char touches_text[32];
        build_score_string(touches_text, "Total Touches", (uint16_t)touches);
        int touches_text_width = strlen(touches_text) * 8 * 4;
        draw_string(touches_text, width/2 - touches_text_width/2, content_start_y, 4, MENU_CONTENT);
        
        instruction_y = content_start_y + 80;
    }
    
    draw_string("Press SPACE to return to shell", width/2 - 248, instruction_y, 2, MENU_CONTENT);
    
    WAIT_SPACE();
}

void countdown_screen(uint64_t size) {
    uint64_t x = width/2 - 4 * size;
    uint64_t y = height/2 - 8 * size;
    draw_string("3", x, y, size, 0xffffff);
    sys_sleep(1000);
    fill_rectangle(x, y, x + 8*size, y + 16*size, BACKGROUND_COLOR_GOLF);
    draw_string("2", x, y, size, 0xffffff);
    sys_sleep(1000);
    fill_rectangle(x, y, x + 8*size, y + 16*size, BACKGROUND_COLOR_GOLF);
    draw_string("1", x, y, size, 0xffffff);
    sys_sleep(1000);
    fill_rectangle(x, y, x + 8*size, y + 16*size, BACKGROUND_COLOR_GOLF);
}

// SCOREBOARD

void draw_scoreboard(uint8_t two_players) {
    // lo relleno siempre porque sino a veces si la pelota colisiona puede haber un frame que borra parte del score
    fill_rectangle(0, 0, width, SCOREBOARD_HEIGHT, MENU_BKG);
    
    // Solo dibujar "Touches: "
    draw_string("Touches: ", 10, 0, 2, MENU_CONTENT);
    
    if (two_players) {
        int text_width = 11 * 8 * 2; // 11 chars, 8 pixels por char, size 2
        int score_x = width - text_width - 10; // 10 pixeles de margen
        
        // Solo dibujar "P1   -   P2"
        draw_string("P1   -   P2", score_x, 0, 2, MENU_CONTENT);
    }
}

void update_scoreboard(uint8_t two_players, uint16_t score1, uint16_t score2, uint64_t touches) {
    // Solo actualizar el número de touches (después de "Touches: ")
    char touch_buffer[32];
    uint64_t touch_len = num_to_str(touches, touch_buffer, 10);
    touch_buffer[touch_len] = '\0';
    
    int touches_number_x = 10 + (9 * 8 * 2); // 9 chars de "Touches: "
    fill_rectangle(touches_number_x, 0, touches_number_x + touch_len * 8 * 2, SCOREBOARD_HEIGHT, MENU_BKG); // 150 pixels debería ser suficiente para cualquier número
    draw_string(touch_buffer, touches_number_x, 0, 2, MENU_CONTENT);
    
    if (two_players) {
        int text_width = 11 * 8 * 2; // 11 chars, 8 pixels por char, size 2
        int score_x = width - text_width - 10; // 10 pixeles de margen
        
        // Solo actualizar los dígitos del score (posiciones 3 y 7 en "P1 x - y P2")
        char score1_buffer[2];
        score1_buffer[0] = '0' + (score1 % 10);
        score1_buffer[1] = '\0';
        
        char score2_buffer[2];
        score2_buffer[0] = '0' + (score2 % 10);
        score2_buffer[1] = '\0';
        
        // Posición del dígito de score1 (después de "P1 ")
        int score1_x = score_x + (3 * 8 * 2); // 3 chars de "P1 "
        fill_rectangle(score1_x, 0, score1_x + 16, SCOREBOARD_HEIGHT, MENU_BKG); // 16 pixels para un char size 2
        draw_string(score1_buffer, score1_x, 0, 2, MENU_CONTENT);
        
        // Posición del dígito de score2 (después de "P1 x - ")
        int score2_x = score_x + (7 * 8 * 2); // 7 chars de "P1 x - "
        fill_rectangle(score2_x, 0, score2_x + 16, SCOREBOARD_HEIGHT, MENU_BKG); // 16 pixels para un char size 2
        draw_string(score2_buffer, score2_x, 0, 2, MENU_CONTENT);
    }
}