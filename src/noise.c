#include "noise.h"
#include <math.h>

/* Hash intero a 32 bit (variante di "lowbias32" di Chris Wellons):
 * distribuisce bene i bit ed e' velocissimo. */
static unsigned int Hash32(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float NoiseHash01(unsigned int seed, int x, int z)
{
    unsigned int h = Hash32((unsigned int)x * 0x9e3779b1u ^
                            (unsigned int)z * 0x85ebca6bu ^
                            seed * 0xc2b2ae35u);
    h = Hash32(h);
    return (float)(h & 0x00ffffffu) / (float)0x00ffffffu;
}

float NoiseLerp(float a, float b, float t) { return a + (b - a) * t; }

float NoiseClamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float NoiseSmoothstep(float edge0, float edge1, float x)
{
    if (edge1 - edge0 == 0.0f) return x < edge0 ? 0.0f : 1.0f;
    float t = NoiseClamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float Noise2D(unsigned int seed, float x, float z)
{
    int xi = (int)floorf(x);
    int zi = (int)floorf(z);
    float xf = x - (float)xi;
    float zf = z - (float)zi;

    /* Curva di Perlin: 6t^5 - 15t^4 + 10t^3 (derivata continua ai bordi). */
    float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    float v = zf * zf * zf * (zf * (zf * 6.0f - 15.0f) + 10.0f);

    float a = NoiseHash01(seed, xi,     zi);
    float b = NoiseHash01(seed, xi + 1, zi);
    float c = NoiseHash01(seed, xi,     zi + 1);
    float d = NoiseHash01(seed, xi + 1, zi + 1);

    return NoiseLerp(NoiseLerp(a, b, u), NoiseLerp(c, d, u), v);
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
