/* ============================================================================
 * entity.h - NPC, creature ostili, IA a stati finiti e proiettili magici.
 * ========================================================================== */
#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "config.h"
#include "world.h"
#include "charmodel.h"
#include "player.h"

/* I tipi di personaggio arrivano da assets/data/entities.txt: 'type' in Entity
 * e' un indice nel catalogo caricato, e si ottiene con EntityFind("wolf").
 * Il comportamento invece resta codice: qui sotto ci sono quelli disponibili,
 * e il file scegle fra questi. */
typedef enum { AI_PEACEFUL, AI_BEAST, AI_HUMANOID } Behaviour;
typedef enum { DRAW_BIPED, DRAW_QUADRUPED } DrawKind;

#define ENT_NONE (-1)

typedef struct {
    char      id[32];
    char      name[28];
    Behaviour behaviour;
    DrawKind  draw;
    char      model[96];        /* "" se il tipo non ha un modello animato */
    Color     color;
    float     maxHp, damage, speed, attackRange, aggro;
    int       xpReward, goldReward;
    char      loot[32];         /* identificatore dell'oggetto, "" se nessuno */
    float     radius, height;
    bool      hostile, persistent, aura;
} EntityDef;

extern EntityDef ENTITY_TYPES[MAX_ENTITY_TYPES];
int  EntityTypeCount(void);
/* Indice del tipo dato l'identificatore, ENT_NONE se non esiste. */
int  EntityFind(const char *id);
bool EntitiesLoadTypes(const char *path);

typedef enum { AI_IDLE, AI_WANDER, AI_CHASE, AI_ATTACK, AI_DEAD } AIState;

typedef struct Entity {
    bool       active;
    int        type;      /* indice in ENTITY_TYPES */
    AIState    state;

    Vector3 pos, vel, home;
    float   yaw, radius, height, speed;

    float hp, maxHp, damage, attackRange, attackCd, stateTimer;
    bool  hostile, onGround, justDied, persistent;

    int   xpReward, goldReward, dropItem;
    int   townIndex;               /* per gli NPC di villaggio */
    char  name[28];

    /* Posa corrente, se per questo tipo esiste un modello animato. */
    CharAnim anim;
    float    animFrame;
} Entity;

typedef struct {
    bool    active;
    Vector3 pos, vel;
    float   life, damage, radius;
    bool    fromPlayer;
} Projectile;

const char *EntityTypeName(int type);
Color       EntityColor(int type);

/* Crea un'entita' nel primo slot libero. Ritorna NULL se l'array e' pieno. */
/* Modelli animati dei personaggi: risorse condivise, non dipendono dal seme.
 * Si caricano una volta all'avvio; se i file non ci sono si disegnano le
 * primitive di sempre. */
void EntitiesLoadModels(void);
void EntitiesUnloadModels(void);

Entity *EntitySpawn(Entity *ents, int type, Vector3 pos, World *w);

/* Aggiorna IA, movimento e attacchi di tutte le entita'. */
void EntitiesUpdate(Entity *ents, World *w, Player *p, float dt);

/* Spawner dinamico: mantiene un numero di nemici attorno al giocatore. */
void EntitiesPopulate(Entity *ents, World *w, Player *p, float dt);

void EntitiesDraw(Entity *ents, World *w, Camera3D cam, Color tint);

/* Entita' piu' vicina nel mirino (per parlare / bersagliare), cercata a partire
 * da un punto e una direzione: la mira parte dagli occhi del giocatore, che in
 * terza persona non coincidono con la camera. */
Entity *EntityLookedAt(Entity *ents, Vector3 origin, Vector3 dir,
                       float maxDist, bool hostileOnly);

/* Proiettili. */
void ProjSpawn(Projectile *pr, Vector3 pos, Vector3 dir, float dmg, bool fromPlayer);
void ProjUpdate(Projectile *pr, Entity *ents, World *w, Player *p, float dt);
void ProjDraw(Projectile *pr, World *w);

#endif /* ENTITY_H */
