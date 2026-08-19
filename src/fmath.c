#include "fmath.h"

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

float FmHash01(unsigned int seed, int x, int z)
{
    unsigned int h = Hash32((unsigned int)x * 0x9e3779b1u ^
                            (unsigned int)z * 0x85ebca6bu ^
                            seed * 0xc2b2ae35u);
    h = Hash32(h);
    return (float)(h & 0x00ffffffu) / (float)0x00ffffffu;
}

float FmLerp(float a, float b, float t) { return a + (b - a) * t; }

float FmClamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float FmSmoothstep(float edge0, float edge1, float x)
{
    if (edge1 - edge0 == 0.0f) return x < edge0 ? 0.0f : 1.0f;
    float t = FmClamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
