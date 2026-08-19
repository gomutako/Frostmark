/* ============================================================================
 * noise.h - Rumore procedurale deterministico (value noise + fBm + ridged).
 *
 * Nessuna dipendenza esterna: tutto e' costruito su una funzione di hash
 * intera, quindi lo stesso seed produce sempre lo stesso mondo su qualunque
 * macchina. E' il cuore della generazione del mondo aperto.
 * ========================================================================== */
#ifndef NOISE_H
#define NOISE_H

/* Valore pseudo-casuale in [0,1] associato alla cella intera (x,z). */
float NoiseHash01(unsigned int seed, int x, int z);

/* Value noise interpolato, output in [0,1]. */
float Noise2D(unsigned int seed, float x, float z);

/* Somma di ottave (fractal Brownian motion), output ~[0,1]. */
float NoiseFBM(unsigned int seed, float x, float z, int octaves,
               float lacunarity, float gain);

/* Rumore "a creste": ottimo per catene montuose, output ~[0,1]. */
float NoiseRidged(unsigned int seed, float x, float z, int octaves);

/* Utility numeriche condivise. */
float NoiseSmoothstep(float edge0, float edge1, float x);
float NoiseLerp(float a, float b, float t);
float NoiseClamp(float v, float lo, float hi);

#endif /* NOISE_H */
