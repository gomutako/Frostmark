/* ============================================================================
 * entity.h - NPC, creature ostili, IA a stati finiti e proiettili magici.
 * ========================================================================== */
#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "config.h"
#include "world.h"
#include "player.h"

typedef enum {
    ENT_NONE, ENT_VILLAGER, ENT_GUARD, ENT_MERCHANT, ENT_ELDER,
    ENT_WOLF, ENT_BANDIT, ENT_REVENANT, ENT_BOSS
} EntityType;

typedef enum { AI_IDLE, AI_WANDER, AI_CHASE, AI_ATTACK, AI_DEAD } AIState;

typedef struct Entity {
    bool       active;
    EntityType type;
    AIState    state;

    Vector3 pos, vel, home;
    float   yaw, radius, height, speed;

    float hp, maxHp, damage, attackRange, attackCd, stateTimer;
    bool  hostile, onGround, justDied, persistent;

    int   xpReward, goldReward, dropItem;
    int   townIndex;               /* per gli NPC di villaggio */
    char  name[28];
} Entity;

typedef struct {
    bool    active;
    Vector3 pos, vel;
    float   life, damage, radius;
    bool    fromPlayer;
} Projectile;

const char *EntityTypeName(EntityType t);
Color       EntityColor(EntityType t);

/* Crea un'entita' nel primo slot libero. Ritorna NULL se l'array e' pieno. */
Entity *EntitySpawn(Entity *ents, EntityType type, Vector3 pos, World *w);

/* Aggiorna IA, movimento e attacchi di tutte le entita'. */
void EntitiesUpdate(Entity *ents, World *w, Player *p, float dt);

/* Spawner dinamico: mantiene un numero di nemici attorno al giocatore. */
void EntitiesPopulate(Entity *ents, World *w, Player *p, float dt);

void EntitiesDraw(Entity *ents, World *w, Camera3D cam, Color tint);

/* Entita' piu' vicina davanti al giocatore (per parlare / bersagliare). */
Entity *EntityLookedAt(Entity *ents, Camera3D cam, float maxDist, bool hostileOnly);

/* Proiettili. */
void ProjSpawn(Projectile *pr, Vector3 pos, Vector3 dir, float dmg, bool fromPlayer);
void ProjUpdate(Projectile *pr, Entity *ents, World *w, Player *p, float dt);
void ProjDraw(Projectile *pr, World *w);

#endif /* ENTITY_H */
