/* ============================================================================
 * noise.h - Rumore procedurale deterministico (value noise + fBm + ridged).
 *
 * NON fa parte del gioco: sta negli strumenti. Il mondo si cuoce una volta con
 * tools/baker e da quel momento la sorgente di verita' e' assets/world/, non
 * questa funzione (fase 3 del piano in docs/05). Le utilita' numeriche e l'hash
 * intero, che al gioco servono ancora, vivono in src/fmath.h.
 *
 * Nessuna dipendenza esterna: tutto e' costruito sull'hash di src/fmath.c,
 * quindi lo stesso seme produce sempre lo stesso mondo su qualunque macchina.
 * ========================================================================== */
#ifndef NOISE_H
#define NOISE_H

/* Value noise interpolato, output in [0,1]. */
float Noise2D(unsigned int seed, float x, float z);

/* Somma di ottave (fractal Brownian motion), output ~[0,1]. */
float NoiseFBM(unsigned int seed, float x, float z, int octaves,
               float lacunarity, float gain);

/* Rumore "a creste": ottimo per catene montuose, output ~[0,1]. */
float NoiseRidged(unsigned int seed, float x, float z, int octaves);

#endif /* NOISE_H */
