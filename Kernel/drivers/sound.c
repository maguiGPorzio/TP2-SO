// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdint.h>
#include <time.h>
#include "sound.h"
#include "time.h"

#define PIT_BASE_HZ 1193180 // Frecuencia base del PIT (Programmable Interval Timer)
#define PC_SPEAKER_PORT 0x61
#define PIT_CHANNEL2_DATA_PORT 0x42
#define PIT_CONTROL_PORT 0x43
#define SPEAKER_OFF_MASK 0xFC // Máscara para apagar el speaker (poner los bits 0 y 1 en 0)
#define PIT_SQUARE_WAVE_MODE 0xB6 // Configuración para el PIT en modo 3 (onda cuadrada)
#define SPEAKER_ENABLE_BITS 3 // Bits para activar el speaker

extern uint8_t port_reader(uint16_t port);
extern void port_writer(uint16_t port, uint8_t data);

// Apaga el parlante (pone en 0 los bits 0 y 1 del puerto 0x61)
void speaker_off(){
    uint8_t tmp = port_reader(PC_SPEAKER_PORT) & SPEAKER_OFF_MASK;
    port_writer(PC_SPEAKER_PORT, tmp);
}

void speaker_start_tone(uint32_t freq_hz){
    if (freq_hz == 0){ // me estan pidiendo silencio
        speaker_off();
        return;
    }
    // calculo el divisor del PIY
    uint32_t div = PIT_BASE_HZ / freq_hz; // pues frecuencia deseada = PIT_BASE_HZ / div
    // programo el PIT (canal 2)
    port_writer(PIT_CONTROL_PORT, PIT_SQUARE_WAVE_MODE); // Modo 3: square wavw 
    port_writer(PIT_CHANNEL2_DATA_PORT, (uint8_t)(div)); // Byte bajo
    port_writer(PIT_CHANNEL2_DATA_PORT, (uint8_t)(div >> 8)); // Byte alto

    // Abro la puerta del speaker
    uint8_t tmp = port_reader(PC_SPEAKER_PORT); // Lee el estado actual
    if (tmp != (tmp | 3)){ // Si los bits 0 y 1 no estan en 1
        port_writer(PC_SPEAKER_PORT, tmp | SPEAKER_ENABLE_BITS); // los pone en 1 para activar el speaker (hace un or con 00000011)
    }
}

void beep(uint32_t freq_hz, uint64_t duration){
    speaker_start_tone(freq_hz);
    sleep(duration);
    speaker_off();
}