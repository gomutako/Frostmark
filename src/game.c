#include "game.h"
#include "ui.h"
#include "save.h"
#include "noise.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/*  Utilita'                                                                 */
/* ------------------------------------------------------------------------ */

void GameToast(Game *g, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->toast, sizeof(g->toast), fmt, ap);
    va_end(ap);
    g->toastTimer = 4.0f;
}

static void GameSubtitle(Game *g, const char *text)
{
    snprintf(g->subtitle, sizeof(g->subtitle), "%s", text);
    g->subtitleTimer = 5.0f;
}

/* Curva di luce: 0 = notte fonda, 1 = pieno giorno. */
static float DayLight(const Game *g)
{
    float t = g->timeOfDay;                       /* 0 = mezzanotte */
    float d = sinf((t - 0.25f) * 2.0f * PI);      /* alba 0.25, tramonto 0.75 */
    return Clamp(0.5f + d * 0.5f, 0.0f, 1.0f);
}

Color GameAmbientTint(const Game *g)
{
    float l = DayLight(g);
    float k = 0.30f + 0.70f * l;
    /* tonalita' calda all'alba/tramonto, fredda di notte */
    float warm = powf(1.0f - fabsf(l - 0.5f) * 2.0f, 2.0f);
    unsigned char r = (unsigned char)Clamp(255.0f * (k + warm * 0.12f), 0, 255);
    unsigned char gg= (unsigned char)Clamp(255.0f * (k + warm * 0.03f), 0, 255);
    unsigned char b = (unsigned char)Clamp(255.0f * (k * 1.05f - warm * 0.05f), 0, 255);
    return (Color){ r, gg, b, 255 };
}

Color GameSkyColor(const Game *g)
{
    float l = DayLight(g);
    Color night = { 12, 16, 30, 255 };
    Color day   = { 122, 168, 220, 255 };
    Color dusk  = { 196, 122, 84, 255 };
    float warm = powf(1.0f - fabsf(l - 0.5f) * 2.0f, 2.5f);

    Color c;
    c.r = (unsigned char)NoiseLerp(night.r, day.r, l);
    c.g = (unsigned char)NoiseLerp(night.g, day.g, l);
    c.b = (unsigned char)NoiseLerp(night.b, day.b, l);
    c.r = (unsigned char)NoiseLerp(c.r, dusk.r, warm * 0.7f);
    c.g = (unsigned char)NoiseLerp(c.g, dusk.g, warm * 0.7f);
    c.b = (unsigned char)NoiseLerp(c.b, dusk.b, warm * 0.5f);
    c.a = 255;
    return c;
}

/* ------------------------------------------------------------------------ */
/*  Creazione del mondo                                                     */
/* ------------------------------------------------------------------------ */

static void SpawnTownNPCs(Game *g)
{
    World *w = &g->world;
    for (int t = 0; t < w->townCount; t++) {
        Vector3 c = w->towns[t].pos;

        struct { EntityType type; float ang; float rad; } roster[] = {
            { ENT_ELDER,    0.4f, 14.0f },
            { ENT_MERCHANT, 1.9f, 16.0f },
            { ENT_GUARD,    3.2f, 20.0f },
            { ENT_GUARD,    5.0f, 20.0f },
            { ENT_VILLAGER, 2.6f, 26.0f },
            { ENT_VILLAGER, 4.4f, 24.0f },
            { ENT_VILLAGER, 0.9f, 30.0f },
        };
        int n = (int)(sizeof(roster) / sizeof(roster[0]));

        for (int i = 0; i < n; i++) {
            Vector3 p = { c.x + cosf(roster[i].ang) * roster[i].rad, 0.0f,
                          c.z + sinf(roster[i].ang) * roster[i].rad };
            Entity *e = EntitySpawn(g->ents, roster[i].type, p, w);
            if (e) {
                e->townIndex = t;
                /* Un nome piu' "vivo" per gli abitanti. */
                snprintf(e->name, sizeof(e->name), "%s di %s",
                         EntityTypeName(roster[i].type), w->towns[t].name);
            }
        }
    }
}

void GameNewWorld(Game *g, unsigned int seed)
{
    /* Ripulisce eventuali risorse precedenti. */
    WorldUnload(&g->world);
    PlayerUnload(&g->player);
    memset(g->ents,  0, sizeof(g->ents));
    memset(g->projs, 0, sizeof(g->projs));

    WorldInit(&g->world, seed);
    QuestInitAll(g->quests);

    Vector3 spawn = g->world.towns[0].pos;
    spawn = WorldSafeSpawn(&g->world, spawn.x + 26.0f, spawn.z + 26.0f);
    PlayerInit(&g->player, spawn);

    SpawnTownNPCs(g);

    g->timeOfDay = 0.32f;     /* si comincia poco dopo l'alba */
    g->playTime  = 0.0f;
    g->invCursor = 0;
    g->shopCursor = 0;
    g->talkTarget = NULL;

    /* Carica subito i chunk attorno allo spawn, cosi' non si "cade" nel vuoto. */
    for (int i = 0; i < 80; i++)
        WorldUpdateStreaming(&g->world, g->player.pos);

    GameSubtitle(g, TextFormat("%s - le terre di Frostmark", g->world.towns[0].name));
}

void GameInit(Game *g, unsigned int seed)
{
    memset(g, 0, sizeof(Game));
    g->state   = GS_MENU;
    g->running = true;
    g->cam.up  = (Vector3){ 0.0f, 1.0f, 0.0f };
    g->cam.fovy = 70.0f;
    g->cam.projection = CAMERA_PERSPECTIVE;
    EntitiesLoadModels();          /* risorse: non dipendono dal seme */
    GameNewWorld(g, seed);
    g->state = GS_MENU;   /* GameNewWorld non cambia stato, ma restiamo espliciti */
}

void GameShutdown(Game *g)
{
    WorldUnload(&g->world);
    PlayerUnload(&g->player);
    EntitiesUnloadModels();
}

/* ------------------------------------------------------------------------ */
/*  Combattimento e interazione                                             */
/* ------------------------------------------------------------------------ */

static void GiveLoot(Game *g, Entity *e)
{
    Player *p = &g->player;
    PlayerAddXP(p, e->xpReward);
    p->gold += e->goldReward;

    if (e->dropItem != ITEM_NONE) InvAdd(p->inv, e->dropItem, 1);

    if (e->type == ENT_WOLF) {
        p->wolvesKilled++;
        QuestProgress(g, QUEST_WOLVES, 1);
    }
    if (e->type == ENT_BOSS) {
        p->bossKilled = true;
        QuestProgress(g, QUEST_BOSS, 1);
        GameSubtitle(g, "Vald il Sepolto torna al silenzio.");
    }
    GameToast(g, "%s sconfitto  (+%d PE, +%d oro)", e->name, e->xpReward, e->goldReward);
}

static void DoMeleeAttack(Game *g)
{
    Player *p = &g->player;
    if (p->attackCd > 0.0f || p->sta < 10.0f) return;

    p->attackCd = 0.55f;
    p->sta -= 10.0f;
    p->swing = 1.0f;

    Vector3 fwd = PlayerLookDir(p);
    float reach = 3.0f;
    float best = 1e9f;
    Entity *hit = NULL;

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g->ents[i];
        if (!e->active || e->state == AI_DEAD || !e->hostile) continue;
        Vector3 mid = { e->pos.x, e->pos.y + e->height * 0.5f, e->pos.z };
        Vector3 rel = Vector3Subtract(mid, p->pos);
        float d = Vector3Length(rel);
        if (d > reach + e->radius) continue;
        if (Vector3DotProduct(Vector3Normalize(rel), fwd) < 0.45f) continue;  /* cono ~60 gradi */
        if (d < best) { best = d; hit = e; }
    }

    if (hit) {
        float dmg = PlayerAttackDamage(p);
        hit->hp -= dmg;
        if (hit->state == AI_IDLE || hit->state == AI_WANDER) hit->state = AI_CHASE;
        p->skillMelee += (GetRandomValue(0, 6) == 0) ? 1 : 0;
    }
}

static void DoCastSpell(Game *g)
{
    Player *p = &g->player;
    if (p->castCd > 0.0f || p->mp < 12.0f) return;

    p->castCd = 0.75f;
    p->mp -= 12.0f;

    /* Il dardo parte dalla mano, non dalla camera: in terza persona la camera
     * e' dietro le spalle e il proiettile attraverserebbe il giocatore. */
    Vector3 fwd = PlayerLookDir(p);
    Vector3 origin = Vector3Add(PlayerEye(p), Vector3Scale(fwd, 0.8f));
    float dmg = 16.0f * (1.0f + p->skillMagic * 0.04f);
    ProjSpawn(g->projs, origin, fwd, dmg, true);
    p->skillMagic += (GetRandomValue(0, 8) == 0) ? 1 : 0;
}

static void DoInteract(Game *g)
{
    /* 1) NPC nel mirino -> dialogo. */
    Entity *npc = EntityLookedAt(g->ents, PlayerEye(&g->player),
                                 PlayerLookDir(&g->player), 4.5f, false);
    if (npc) {
        g->talkTarget  = npc;
        g->dialogueOpt = 0;
        DialogueBuild(g, npc, &g->dlg);
        g->state = GS_DIALOGUE;
        EnableCursor();
        return;
    }

    /* 2) Erba raccoglibile vicina. */
    Prop *herb = WorldNearestProp(&g->world, g->player.pos, 3.0f, PROP_HERB);
    if (herb) {
        herb->taken = true;
        InvAdd(g->player.inv, ItemFind("herb"), 1);
        g->player.herbsPicked++;
        QuestProgress(g, QUEST_HERBS, 1);
        if (g->quests[QUEST_HERBS].state != Q_ACTIVE)
            GameToast(g, "Hai raccolto: %s", ITEMS[ItemFind("herb")].name);
        return;
    }

    GameToast(g, "Non c'e' nulla con cui interagire.");
}

/* Risveglia il boss quando il giocatore si avvicina alla cripta. */
static void UpdateCrypt(Game *g)
{
    static bool spawned = false;
    if (g->player.bossKilled) return;

    float d = Vector3Distance(g->player.pos, g->world.cryptPos);
    if (!spawned && d < 55.0f) {
        Entity *boss = EntitySpawn(g->ents, ENT_BOSS, g->world.cryptPos, &g->world);
        if (boss) {
            for (int k = 0; k < 3; k++) {
                float a = (float)k * 2.1f;
                Vector3 p = { g->world.cryptPos.x + cosf(a) * 12.0f, 0.0f,
                              g->world.cryptPos.z + sinf(a) * 12.0f };
                Entity *m = EntitySpawn(g->ents, ENT_REVENANT, p, &g->world);
                if (m) m->persistent = true;
            }
            spawned = true;
            GameSubtitle(g, "La terra trema: qualcosa si e' svegliato.");
        }
    }
    if (spawned && g->player.bossKilled) spawned = false;
}

/* ------------------------------------------------------------------------ */
/*  Aggiornamento                                                            */
/* ------------------------------------------------------------------------ */

static void UpdatePlaying(Game *g, float dt)
{
    Player *p = &g->player;

    g->playTime  += dt;
    g->timeOfDay += dt / DAY_LENGTH_SECONDS;
    if (g->timeOfDay >= 1.0f) g->timeOfDay -= 1.0f;

    PlayerUpdate(p, &g->world, dt, true);
    PlayerCamera(p, &g->world, &g->cam);

    WorldUpdateStreaming(&g->world, p->pos);
    EntitiesPopulate(g->ents, &g->world, p, dt);
    EntitiesUpdate(g->ents, &g->world, p, dt);
    ProjUpdate(g->projs, g->ents, &g->world, p, dt);
    UpdateCrypt(g);

    /* Ricompense per le entita' appena morte. */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (g->ents[i].active && g->ents[i].justDied) {
            g->ents[i].justDied = false;
            g->ents[i].stateTimer = 0.0f;
            GiveLoot(g, &g->ents[i]);
        }
    }

    if (p->hp <= 0.0f) {
        g->state = GS_DEAD;
        EnableCursor();
    }
}

static void RespawnPlayer(Game *g)
{
    /* Rinasce al villaggio piu' vicino, perdendo un quarto dell'oro. */
    World *w = &g->world;
    int best = 0;
    float bd = 1e30f;
    for (int i = 0; i < w->townCount; i++) {
        float d = Vector3Distance(g->player.pos, w->towns[i].pos);
        if (d < bd) { bd = d; best = i; }
    }
    Vector3 s = WorldSafeSpawn(w, w->towns[best].pos.x + 18.0f, w->towns[best].pos.z + 18.0f);
    g->player.pos = s;
    g->player.vel = (Vector3){ 0 };
    g->player.hp  = g->player.maxHp * 0.5f;
    g->player.sta = g->player.maxSta;
    g->player.gold = (int)(g->player.gold * 0.75f);
    g->player.hurtFlash = 0.0f;

    for (int i = 0; i < 60; i++) WorldUpdateStreaming(w, g->player.pos);
    GameToast(g, "Ti sei risvegliato a %s.", w->towns[best].name);
}

/* ------------------------------------------------------------------------ */
/*  AGGIORNAMENTO: SIMULAZIONE E INPUT SONO SEPARATI                        */
/*                                                                          */
/*  La simulazione gira a passo fisso (vedi main.c) e puo' essere eseguita   */
/*  piu' volte in un fotogramma; l'input una volta sola. Tenerli insieme     */
/*  farebbe scattare due volte lo stesso clic quando il passo recupera.      */
/* ------------------------------------------------------------------------ */

void GameSimulate(Game *g, float dt)
{
    if (g->toastTimer    > 0.0f) g->toastTimer    -= dt;
    if (g->subtitleTimer > 0.0f) g->subtitleTimer -= dt;

    if (g->state == GS_PLAY) {
        UpdatePlaying(g, dt);
    } else if (g->state == GS_DEAD) {
        /* Il giocatore non si aggiorna piu', ma l'animazione di morte deve
         * arrivare a fine corsa. */
        PlayerUpdateAnimation(&g->player, dt);
    }
}

void GameInput(Game *g)
{
    switch (g->state) {

    case GS_MENU:
        if (IsKeyPressed(KEY_ENTER)) {
            GameNewWorld(g, g->world.seed);
            g->state = GS_PLAY;
            DisableCursor();
        } else if (IsKeyPressed(KEY_S)) {
            unsigned int s = (unsigned int)(GetTime() * 100000.0) ^ 0xA53Fu;
            GameNewWorld(g, s);
            GameToast(g, "Nuovo mondo generato (seme %u)", s);
        } else if (IsKeyPressed(KEY_C)) {
            if (LoadGameFromFile(g, SAVE_FILE)) {
                g->state = GS_PLAY;
                DisableCursor();
                GameToast(g, "Partita caricata.");
            } else GameToast(g, "Nessun salvataggio valido trovato.");
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            g->running = false;
        }
        break;

    case GS_PLAY:
        if (IsKeyPressed(KEY_ESCAPE))    { g->state = GS_PAUSE;     EnableCursor(); break; }
        if (IsKeyPressed(KEY_TAB))       { g->state = GS_INVENTORY; EnableCursor(); break; }
        if (IsKeyPressed(KEY_J))         { g->state = GS_JOURNAL;   EnableCursor(); break; }
        if (IsKeyPressed(KEY_M))         { g->state = GS_MAP;       EnableCursor(); break; }

        /* Combattimento e interazione: una volta per fotogramma. */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))  DoMeleeAttack(g);
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) DoCastSpell(g);
        if (IsKeyPressed(KEY_E)) DoInteract(g);
        if (IsKeyPressed(KEY_R)) {   /* bere rapidamente una pozione */
            Player *p = &g->player;
            int potion = ItemFind("potion_health");
            if (InvCount(p->inv, potion) > 0) {
                p->hp = fminf(p->maxHp, p->hp + ITEMS[potion].power);
                InvRemove(p->inv, potion, 1);
                GameToast(g, "Hai bevuto una pozione di cura.");
            } else GameToast(g, "Non hai pozioni di cura.");
        }
        break;

    case GS_PAUSE:
        if (IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); }
        if (IsKeyPressed(KEY_F5))
            GameToast(g, SaveGameToFile(g, SAVE_FILE) ? "Partita salvata."
                                                      : "Salvataggio fallito.");
        if (IsKeyPressed(KEY_F9)) {
            if (LoadGameFromFile(g, SAVE_FILE)) { GameToast(g, "Partita caricata."); g->state = GS_PLAY; DisableCursor(); }
            else GameToast(g, "Nessun salvataggio valido.");
        }
        if (IsKeyPressed(KEY_Q)) { g->state = GS_MENU; EnableCursor(); }
        break;

    case GS_INVENTORY: {
        Player *p = &g->player;
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); break; }
        if (IsKeyPressed(KEY_DOWN)) {
            for (int i = g->invCursor + 1; i < MAX_INVENTORY; i++)
                if (p->inv[i].id != ITEM_NONE) { g->invCursor = i; break; }
        }
        if (IsKeyPressed(KEY_UP)) {
            for (int i = g->invCursor - 1; i >= 0; i--)
                if (p->inv[i].id != ITEM_NONE) { g->invCursor = i; break; }
        }
        if (IsKeyPressed(KEY_ENTER)) {
            int id = p->inv[g->invCursor].id;
            if (PlayerUseItem(p, g->invCursor)) GameToast(g, "%s", ITEMS[id].name);
        }
        if (IsKeyPressed(KEY_X)) {
            int id = p->inv[g->invCursor].id;
            if (id != ITEM_NONE && id != p->weapon && id != p->armor) {
                InvRemove(p->inv, id, 1);
                GameToast(g, "Hai gettato: %s", ITEMS[id].name);
            }
        }
    } break;

    case GS_JOURNAL:
        if (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); }
        break;

    case GS_MAP:
        if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); }
        break;

    case GS_DIALOGUE:
        if (IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); break; }
        if (IsKeyPressed(KEY_DOWN) && g->dlg.optCount > 0)
            g->dialogueOpt = (g->dialogueOpt + 1) % g->dlg.optCount;
        if (IsKeyPressed(KEY_UP) && g->dlg.optCount > 0)
            g->dialogueOpt = (g->dialogueOpt + g->dlg.optCount - 1) % g->dlg.optCount;
        if (IsKeyPressed(KEY_ENTER)) {
            DialogueChoose(g, g->talkTarget, g->dialogueOpt);
            if (g->state == GS_PLAY) DisableCursor();
        }
        for (int k = 0; k < g->dlg.optCount; k++) {
            if (IsKeyPressed(KEY_ONE + k)) {
                DialogueChoose(g, g->talkTarget, k);
                if (g->state == GS_PLAY) DisableCursor();
                break;
            }
        }
        break;

    case GS_SHOP: {
        Player *p = &g->player;
        if (IsKeyPressed(KEY_ESCAPE)) { g->state = GS_PLAY; DisableCursor(); break; }
        if (IsKeyPressed(KEY_DOWN)) g->shopCursor = (g->shopCursor + 1) % SHOP_STOCK_COUNT;
        if (IsKeyPressed(KEY_UP))   g->shopCursor = (g->shopCursor + SHOP_STOCK_COUNT - 1) % SHOP_STOCK_COUNT;
        if (IsKeyPressed(KEY_ENTER)) {
            int id = SHOP_STOCK[g->shopCursor];
            if (p->gold >= ITEMS[id].value) {
                if (InvAdd(p->inv, id, 1)) {
                    p->gold -= ITEMS[id].value;
                    GameToast(g, "Acquistato: %s", ITEMS[id].name);
                } else GameToast(g, "Zaino pieno.");
            } else GameToast(g, "Oro insufficiente.");
        }
        if (IsKeyPressed(KEY_V)) {
            int id = SHOP_STOCK[g->shopCursor];
            if (InvCount(p->inv, id) > 0 && id != p->weapon && id != p->armor) {
                InvRemove(p->inv, id, 1);
                p->gold += ITEMS[id].value / 2;
                GameToast(g, "Venduto: %s (+%d oro)", ITEMS[id].name, ITEMS[id].value / 2);
            } else GameToast(g, "Non hai nulla da vendere qui.");
        }
    } break;

    case GS_DEAD:
        if (IsKeyPressed(KEY_ENTER)) { RespawnPlayer(g); g->state = GS_PLAY; DisableCursor(); }
        if (IsKeyPressed(KEY_Q))     { g->state = GS_MENU; }
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*  Disegno                                                                  */
/* ------------------------------------------------------------------------ */

/* Sole/luna come sfera lontana: un tocco di atmosfera a costo zero. */
static void DrawSkyBody(Game *g)
{
    float ang = (g->timeOfDay - 0.25f) * 2.0f * PI;
    Vector3 dir = { cosf(ang) * 0.6f, sinf(ang), 0.45f };
    dir = Vector3Normalize(dir);
    Vector3 pos = Vector3Add(g->cam.position, Vector3Scale(dir, 320.0f));
    bool isDay = DayLight(g) > 0.35f;
    DrawSphere(pos, isDay ? 16.0f : 11.0f,
               isDay ? (Color){ 255, 244, 200, 255 } : (Color){ 218, 224, 238, 255 });
}

static void DrawScene(Game *g)
{
    Color tint = GameAmbientTint(g);

    BeginMode3D(g->cam);
        DrawSkyBody(g);
        WorldDrawTerrain(&g->world, g->cam, tint);
        WorldDrawProps(&g->world, g->cam, tint);
        EntitiesDraw(g->ents, &g->world, g->cam, tint);
        ProjDraw(g->projs, &g->world);
        WorldDrawWater(&g->world, g->player.pos, tint, g->playTime);

        /* Il corpo del giocatore si vede solo in terza persona. */
        PlayerDraw(&g->player, tint);

        /* Arma in prima persona: un semplice parallelepipedo che oscilla. */
        if (g->state == GS_PLAY && g->player.camMode == CAM_FIRST) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(g->cam.target, g->cam.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, g->cam.up));
            float sw = g->player.swing;
            Vector3 hand = Vector3Add(g->cam.position, Vector3Scale(fwd, 1.30f + sw * 0.30f));
            hand = Vector3Add(hand, Vector3Scale(right, 0.46f - sw * 0.22f));
            hand.y -= 0.44f - sw * 0.14f;
            /* lama */
            DrawCube(hand, 0.045f, 0.045f, 0.50f, (Color){ 206, 208, 216, 255 });
            /* elsa, spostata verso il giocatore */
            Vector3 hilt = Vector3Subtract(hand, Vector3Scale(fwd, 0.28f));
            DrawCube(hilt, 0.07f, 0.07f, 0.10f, (Color){ 92, 66, 44, 255 });
        }
    EndMode3D();
}

void GameDraw(Game *g)
{
    if (g->state == GS_MENU) {
        UIDrawMenu(g);
        return;
    }

    ClearBackground(GameSkyColor(g));
    DrawScene(g);
    UIDrawWorldMarkers(g);
    UIDrawHUD(g);

    switch (g->state) {
        case GS_PLAY:      UIDrawCrosshair(g);  break;
        case GS_PAUSE:     UIDrawPause(g);      break;
        case GS_INVENTORY: UIDrawInventory(g);  break;
        case GS_JOURNAL:   UIDrawJournal(g);    break;
        case GS_MAP:       UIDrawMap(g);        break;
        case GS_DIALOGUE:  UIDrawDialogue(g);   break;
        case GS_SHOP:      UIDrawShop(g);       break;
        case GS_DEAD:      UIDrawDeath(g);      break;
        default: break;
    }

    DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 24);
}
