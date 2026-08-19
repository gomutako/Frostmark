/* ============================================================================
 * fmath.h - Utilita' numeriche condivise: hash deterministico e interpolazioni.
 *
 * Nasce dalla divisione di noise.c (fase 3 del piano in docs/05): il rumore
 * procedurale che generava il terreno se ne va negli strumenti, perche' il
 * mondo ora si cuoce una volta e si carica. Qui resta cio' che serve ancora al
 * gioco: le interpolazioni usate da mesh, colori e ciclo del giorno, e l'hash
 * intero con cui gli NPC prendono decisioni riproducibili.
 *
 * L'hash e' lo stesso di prima, bit per bit: cambiarlo cambierebbe il mondo
 * cotto e il comportamento degli NPC senza motivo.
 * ========================================================================== */
#ifndef FMATH_H
#define FMATH_H

/* Valore pseudo-casuale in [0,1] associato alla cella intera (x,z). */
float FmHash01(unsigned int seed, int x, int z);

float FmSmoothstep(float edge0, float edge1, float x);
float FmLerp(float a, float b, float t);
float FmClamp(float v, float lo, float hi);

#endif /* FMATH_H */
