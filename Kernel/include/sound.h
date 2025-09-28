#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>

void speaker_off();
void speaker_start_tone(uint32_t freq_hz);
void beep(uint32_t freq_hz, uint64_t duration);

#endif