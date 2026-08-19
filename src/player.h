/* ============================================================================
 * player.h - Stato e movimento del giocatore, camera in prima/terza persona.
 * ========================================================================== */
#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "config.h"
#include "items.h"
#include "world.h"

/* La visuale e' una preferenza di vista: non finisce nel salvataggio. */
typedef enum { CAM_FIRST, CAM_THIRD } CamMode;

/* Ruoli di animazione. Un pacchetto di personaggi ne offre spesso decine (il
 * cavaliere KayKit ne ha 76): al gioco servono questi, e PlayerLoadModel() li
 * cerca per nome. */
typedef enum {
    PANIM_IDLE, PANIM_WALK, PANIM_RUN, PANIM_ATTACK, PANIM_BLOCK,
    PANIM_CAST, PANIM_JUMP, PANIM_HURT, PANIM_DEATH, PANIM_COUNT
} PlayerAnim;

typedef struct {
    Vector3 pos, vel;
    float   yaw, pitch;          /* radianti */
    bool    onGround, sprinting, inWater;

    CamMode camMode;
    float   camDist;             /* distanza della camera in terza persona */
    bool    blocking;            /* guardia alzata (CTRL sinistro)         */
    float   moveSpeed;           /* velocita' orizzontale voluta, m/s      */

    /* Modello animato opzionale: se assets/models/player.glb non c'e', in
     * terza persona si disegnano le primitive. */
    Model            model;
    /* Vista del modello con le sole mesh dotate di scheletro: serve perche'
     * UpdateModelAnimation() di raylib non regge i modelli misti (vedi
     * BuildSkinnedView in player.c). Condivide i dati con 'model'. */
    Model            skinnedView;
    bool             viewAllocated;
    /* Mesh senza pesi da muovere a mano seguendo un osso. */
    struct { int mesh, bone; } attach[MAX_ATTACHMENTS];
    int              attachCount;

    ModelAnimation  *anims;
    int              animCount;
    int              animOf[PANIM_COUNT];   /* indice in anims, -1 se assente */
    PlayerAnim       anim;
    float            animFrame;
    bool             hasModel, anySkinned;

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
/* Rilascia il modello animato: da chiamare prima di reinizializzare il
 * giocatore e alla chiusura. */
void  PlayerUnload(Player *p);
void  PlayerUpdate(Player *p, World *w, float dt, bool controlsEnabled);

/* La camera in terza persona interroga l'altezza del terreno per non
 * sprofondare, quindi serve il mondo. */
void  PlayerCamera(const Player *p, const World *w, Camera3D *cam);
/* Disegna il corpo: non fa nulla in prima persona. */
void  PlayerDraw(const Player *p, Color tint);
/* Fa scorrere l'animazione. PlayerUpdate() la chiama da se': serve a parte solo
 * quando il gioco non aggiorna il giocatore ma la posa deve muoversi ancora,
 * cioe' nella schermata di morte. */
void  PlayerUpdateAnimation(Player *p, float dt);

/* Mira e interazione partono sempre dagli occhi del giocatore, non dalla
 * camera: in terza persona la camera e' arretrata di qualche metro. */
Vector3 PlayerEye(const Player *p);
Vector3 PlayerLookDir(const Player *p);
void  PlayerAddXP(Player *p, int amount);   /* gestisce anche il level-up */
float PlayerAttackDamage(const Player *p);
float PlayerArmorValue(const Player *p);
void  PlayerTakeDamage(Player *p, float dmg);
bool  PlayerUseItem(Player *p, int slotIndex);   /* consuma/equipaggia */

#endif /* PLAYER_H */
