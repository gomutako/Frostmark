/* ============================================================================
 * balance.h - Numeri di bilanciamento, caricati da assets/data/balance.txt.
 *
 * Sono i valori che si ritoccano provando: stanno in un file per poterli
 * cambiare senza ricompilare. Non hanno valori di ripiego nel codice.
 * ========================================================================== */
#ifndef BALANCE_H
#define BALANCE_H

#include <stdbool.h>

typedef struct {
    float walk, run, jump, gravity;
    float eyeHeight, bodyHeight, radius;

    float blockSpeedMul, blockDamageMul, blockStaminaDrain;

    float staminaSprint, staminaRegen, staminaJump;
    float manaRegen, healthRegen;

    float fallThreshold, fallFactor;

    int   xpBase, xpPerLevel;
    float hpPerLevel, staPerLevel, mpPerLevel;

    float daySeconds;

    /* Radianti di rotazione per unita' di movimento del mouse. Col movimento
     * grezzo (vedi rawmouse.h) l'unita' e' un conteggio del dispositivo, non
     * un pixel: dipende dai DPI, quindi si tara qui. */
    float mouseSens;
} Balance;

extern Balance BAL;

bool BalanceLoad(const char *path);

#endif /* BALANCE_H */
