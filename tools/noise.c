#include "noise.h"
#include "fmath.h"
#include <math.h>

float Noise2D(unsigned int seed, float x, float z)
{
    int xi = (int)floorf(x);
    int zi = (int)floorf(z);
    float xf = x - (float)xi;
    float zf = z - (float)zi;

    /* Curva di Perlin: 6t^5 - 15t^4 + 10t^3 (derivata continua ai bordi). */
    float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    float v = zf * zf * zf * (zf * (zf * 6.0f - 15.0f) + 10.0f);

    float a = FmHash01(seed, xi,     zi);
    float b = FmHash01(seed, xi + 1, zi);
    float c = FmHash01(seed, xi,     zi + 1);
    float d = FmHash01(seed, xi + 1, zi + 1);

    return FmLerp(FmLerp(a, b, u), FmLerp(c, d, u), v);
}

float NoiseFBM(unsigned int seed, float x, float z, int octaves,
               float lacunarity, float gain)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        sum  += amp * Noise2D(seed + (unsigned int)i * 7919u, x * freq, z * freq);
        norm += amp;
        amp  *= gain;
        freq *= lacunarity;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}

float NoiseRidged(unsigned int seed, float x, float z, int octaves)
{
    float sum = 0.0f, amp = 0.5f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        float n = Noise2D(seed + (unsigned int)i * 6151u, x * freq, z * freq);
        n = 1.0f - fabsf(n * 2.0f - 1.0f);   /* piega il rumore: crea creste */
        n *= n;
        sum  += n * amp;
        norm += amp;
        amp  *= 0.5f;
        freq *= 2.0f;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}
