#include <stdint.h>

// Random
static uint32_t m_z = 362436069;
static uint32_t m_w = 521288629;

uint32_t GetUint() {
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  return (m_z << 16) + m_w;
}

uint32_t GetUniform(uint32_t max) {
  uint32_t u = GetUint();
  return (u + 1.0) * 2.328306435454494e-10 * max;
}

float inv_sqrt(float number){
    const float threehalfs = 1.5F;
    float x2 = number * 0.5F;

    union { float f; uint32_t i; } conv = { .f = number };
    conv.i  = 0x5f3759df - (conv.i >> 1);  
    float y = conv.f;

    y = y * (threehalfs - (x2 * y * y));
    return y;             
}