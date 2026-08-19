/* ============================================================================
 * player.h - Stato e movimento del giocatore (prima persona).
 * ========================================================================== */
#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "config.h"
#include "items.h"
#include "world.h"

typedef struct {
    Vector3 pos, vel;
    float   yaw, pitch;          /* radianti */
    bool    onGround, sprinting, inWater;

    float   hp, maxHp;
    float   sta, maxSta;
    float   mp, maxMp;

    int     level, xp, xpNext, gold;
    int     skillMelee, skillMagic;

    InvSlot inv[MAX_INVENTORY];
    int     weapon, armor;       /* ID oggetto equipaggiato, 0 = nudo */

    float   attackCd, castCd, hurtFlash, swing, bobPhase;
    int     wolvesKilled, herbsPicked;
    bool    bossKilled;
} Player;

void  PlayerInit(Player *p, Vector3 spawn);
void  PlayerUpdate(Player *p, World *w, float dt, bool controlsEnabled);
void  PlayerCamera(const Player *p, Camera3D *cam);
void  PlayerAddXP(Player *p, int amount);   /* gestisce anche il level-up */
float PlayerAttackDamage(const Player *p);
float PlayerArmorValue(const Player *p);
void  PlayerTakeDamage(Player *p, float dmg);
bool  PlayerUseItem(Player *p, int slotIndex);   /* consuma/equipaggia */

#endif /* PLAYER_H */
