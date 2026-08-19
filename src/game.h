/* ============================================================================
 * game.h - Stato globale, macchina a stati dell'interfaccia, ciclo di gioco.
 * ========================================================================== */
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "config.h"
#include "world.h"
#include "player.h"
#include "entity.h"
#include "quest.h"
#include "items.h"

typedef enum {
    GS_MENU, GS_PLAY, GS_PAUSE, GS_INVENTORY,
    GS_DIALOGUE, GS_SHOP, GS_JOURNAL, GS_MAP, GS_DEAD
} GameState;

typedef struct Game {
    GameState  state;
    World      world;
    Player     player;
    Entity     ents[MAX_ENTITIES];
    Projectile projs[MAX_PROJECTILES];
    QuestInst  quests[MAX_QUESTS];

    Camera3D   cam;
    float      timeOfDay;      /* 0..1, 0.25 = alba, 0.5 = mezzogiorno */
    float      playTime;
    bool       running;

    /* UI */
    int        invCursor;
    int        shopCursor;
    int        dialogueOpt;
    Entity    *talkTarget;
    Dialogue   dlg;

    char       toast[160];
    float      toastTimer;
    char       subtitle[96];
    float      subtitleTimer;
} Game;

void GameInit(Game *g, unsigned int seed);
void GameNewWorld(Game *g, unsigned int seed);
void GameShutdown(Game *g);
/* Input una volta per fotogramma, simulazione a passo fisso: vedi main.c. */
void GameInput(Game *g);
void GameSimulate(Game *g, float dt);
void GameDraw(Game *g);

void GameToast(Game *g, const char *fmt, ...);
Color GameAmbientTint(const Game *g);
Color GameSkyColor(const Game *g);

#endif /* GAME_H */
