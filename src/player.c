#include "player.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define MOUSE_SENS 0.0026f

/* ------------------------------------------------------------------------ */
/*  ANIMAZIONI                                                              */
/*                                                                          */
/*  Il modello e la ricerca delle clip stanno in charmodel.c, condivisi con  */
/*  gli NPC. Qui c'e' solo la scelta del ruolo dallo stato del giocatore.    */
/* ------------------------------------------------------------------------ */

/* Quale ruolo mostrare, in ordine di priorita'. */
static CharAnim PickAnim(const Player *p)
{
    if (p->hp <= 0.0f)        return CANIM_DEATH;
    if (p->hurtFlash > 0.22f) return CANIM_HURT;
    if (p->swing > 0.05f)     return CANIM_ATTACK;
    if (p->castCd > 0.50f)    return CANIM_CAST;   /* castCd parte da 0.75 */
    if (p->blocking)          return CANIM_BLOCK;
    if (!p->onGround)         return CANIM_JUMP;
    if (p->moveSpeed > 0.2f)  return p->sprinting ? CANIM_RUN : CANIM_WALK;
    return CANIM_IDLE;
}

/* Durata a schermo delle animazioni non ciclabili: quanto dura l'azione nel
 * gioco, non quanto e' lunga la clip. Il fendente del cavaliere KayKit dura 59
 * fotogrammi, cioe' circa un secondo, mentre il colpo si esaurisce in 0.4 s:
 * senza questo la spada si fermerebbe a meta' del movimento. */
static float OneShotSeconds(CharAnim a)
{
    switch (a) {
        case CANIM_ATTACK: return 0.42f;
        case CANIM_CAST:   return 0.30f;
        case CANIM_HURT:   return 0.28f;
        case CANIM_DEATH:  return 1.30f;
        default:           return 1.00f;
    }
}

void PlayerUpdateAnimation(Player *p, float dt)
{
    if (!p->model.loaded) return;

    CharAnim want = CharModelResolve(&p->model, PickAnim(p));
    if (want != p->anim) { p->anim = want; p->animFrame = 0.0f; }

    /* Il passo segue la velocita' reale, cosi' i piedi non slittano. */
    float speedMul = 1.0f;
    if (want == CANIM_WALK || want == CANIM_RUN)
        speedMul = p->moveSpeed / ((want == CANIM_RUN) ? PLAYER_RUN : PLAYER_WALK);

    p->animFrame = CharModelAdvance(&p->model, want, p->animFrame, dt,
                                    speedMul, OneShotSeconds(want));
}

void PlayerUnload(Player *p)
{
    CharModelUnload(&p->model);
}

/* ------------------------------------------------------------------------ */

void PlayerInit(Player *p, Vector3 spawn)
{
    memset(p, 0, sizeof(Player));
    p->pos   = spawn;
    p->yaw   = 0.0f;
    p->pitch = -0.05f;

    p->camMode = CAM_FIRST;
    p->camDist = CAM_DIST_DEF;

    p->maxHp = 100.0f; p->hp  = p->maxHp;
    p->maxSta= 100.0f; p->sta = p->maxSta;
    p->maxMp =  60.0f; p->mp  = p->maxMp;

    p->level = 1; p->xp = 0; p->xpNext = 120; p->gold = 25;
    p->skillMelee = 5; p->skillMagic = 5;

    /* Dotazione iniziale. Gli oggetti si cercano per identificatore: la
      * tabella e' in assets/data/items.txt. Quali oggetti dare all'inizio
      * diventera' un dato con la fase 2 del piano (docs/05). */
    p->weapon = ItemFind("rusty_sword");
    p->armor  = ITEM_NONE;

    InvAdd(p->inv, ItemFind("rusty_sword"), 1);
    InvAdd(p->inv, ItemFind("potion_health"), 2);
    InvAdd(p->inv, ItemFind("bread"), 3);

    /* Modello animato opzionale: la scala viene ricavata dall'altezza voluta.
     * Va dopo il memset, che azzera la struttura. */
    CharModelLoad(&p->model, PLAYER_MODEL_FILE, PLAYER_HEIGHT);
}

Vector3 PlayerEye(const Player *p)
{
    return (Vector3){ p->pos.x, p->pos.y + PLAYER_EYE, p->pos.z };
}

Vector3 PlayerLookDir(const Player *p)
{
    return (Vector3){ cosf(p->pitch) * sinf(p->yaw),
                      sinf(p->pitch),
                      cosf(p->pitch) * cosf(p->yaw) };
}

void PlayerCamera(const Player *p, const World *w, Camera3D *cam)
{
    /* Leggero "head bob" quando si cammina: costa poco, aggiunge molto. In
     * terza persona darebbe il mal di mare, quindi vale solo in soggettiva. */
    float bob = (p->camMode == CAM_FIRST) ? sinf(p->bobPhase) * 0.055f : 0.0f;
    Vector3 eye = PlayerEye(p);
    eye.y += bob;
    Vector3 fwd = PlayerLookDir(p);

    cam->up   = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->fovy = 70.0f;
    cam->projection = CAMERA_PERSPECTIVE;

    if (p->camMode == CAM_FIRST) {
        cam->position = eye;
        cam->target   = Vector3Add(eye, fwd);
        return;
    }

    /* Terza persona: la camera arretra lungo la direzione della visuale.
     * Se il terreno si mette in mezzo (siamo in salita, o guardiamo in alto)
     * la distanza si accorcia, altrimenti la camera finisce sotto la collina.
     * Il campionamento e' grossolano - otto passi - ma il terreno e' una
     * funzione pura, quindi costa solo qualche WorldHeight() per frame. */
    const int STEPS = 8;
    float dist = p->camDist;
    for (int i = 1; i <= STEPS; i++) {
        float t = p->camDist * (float)i / (float)STEPS;
        Vector3 s = Vector3Subtract(eye, Vector3Scale(fwd, t));
        if (s.y < WorldHeight(w, s.x, s.z) + CAM_CLEARANCE) {
            dist = t - p->camDist / (float)STEPS;   /* fermati un passo prima */
            break;
        }
    }
    if (dist < 0.5f) dist = 0.5f;

    /* Scostamento in alto e a destra: senza, il giocatore sta esattamente sul
     * mirino e copre quello che si sta inquadrando. Il prezzo e' che l'asse
     * della camera non coincide piu' con la direzione di mira - qui sono 3
     * gradi - ma il combattimento non ne soffre: usa PlayerLookDir(), non la
     * camera. */
    Vector3 right = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };
    cam->position = Vector3Subtract(eye, Vector3Scale(fwd, dist));
    cam->position = Vector3Add(cam->position, Vector3Scale(right, CAM_SHOULDER));
    cam->position.y += CAM_RISE;

    float minY = WorldHeight(w, cam->position.x, cam->position.z) + CAM_CLEARANCE;
    if (cam->position.y < minY) cam->position.y = minY;

    cam->target = Vector3Add(eye, Vector3Scale(fwd, 4.0f));
}

/* Disegna il modello animato. Con uno scheletro si disegnano solo le mesh
 * skinnate (vedi PlayerLoadModel), altrimenti tutte. */
void PlayerDraw(const Player *p, Color tint)
{
    if (p->camMode == CAM_FIRST) return;

    if (p->model.loaded) {
        CharModelDraw(&p->model, p->pos, p->yaw + PLAYER_MODEL_YAW * DEG2RAD,
                      p->anim, p->animFrame, tint);
        return;
    }

    Color body = { (unsigned char)( 96 * tint.r / 255),
                   (unsigned char)(104 * tint.g / 255),
                   (unsigned char)(126 * tint.b / 255), 255 };
    Color skin = { (unsigned char)(216 * tint.r / 255),
                   (unsigned char)(186 * tint.g / 255),
                   (unsigned char)(152 * tint.b / 255), 255 };

    /* Stessa grammatica visiva dei bipedi in entity.c: capsula, testa, arma.
     * Il raggio e' piu' sottile di PLAYER_RADIUS, che serve alle collisioni:
     * disegnarlo a 0.45 m dava un personaggio a forma di botte che copriva
     * anche la spada. */
    const float BODY_R = 0.30f;
    Vector3 f = { sinf(p->yaw), 0.0f, cosf(p->yaw) };
    Vector3 r = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };

    Vector3 a = { p->pos.x, p->pos.y + 0.45f, p->pos.z };
    Vector3 b = { p->pos.x, p->pos.y + PLAYER_HEIGHT - 0.55f, p->pos.z };
    DrawCapsule(a, b, BODY_R, 10, 6, body);
    DrawSphere((Vector3){ p->pos.x, p->pos.y + PLAYER_HEIGHT - 0.22f, p->pos.z },
               0.24f, skin);

    /* Gambe: due parallelepipedi che oscillano su bobPhase, la stessa fase che
     * in soggettiva muove la testa. Non e' un'animazione vera, ma basta per
     * leggere il passo e per capire da che parte e' girato il personaggio. */
    Color legc = { (unsigned char)(body.r * 3 / 4), (unsigned char)(body.g * 3 / 4),
                   (unsigned char)(body.b * 3 / 4), 255 };
    float step = sinf(p->bobPhase) * 0.20f;
    for (int i = 0; i < 2; i++) {
        float side = (i == 0) ? 1.0f : -1.0f;
        Vector3 leg = { p->pos.x + r.x * 0.15f * side + f.x * step * side,
                        p->pos.y + 0.28f,
                        p->pos.z + r.z * 0.15f * side + f.z * step * side };
        DrawCube(leg, 0.17f, 0.56f, 0.17f, legc);
    }

    if (p->weapon != ITEM_NONE) {
        float sw = p->swing;                     /* 1 -> 0 durante il fendente */
        Vector3 hand = { p->pos.x + r.x * 0.42f + f.x * (0.20f + sw * 0.45f),
                         p->pos.y + PLAYER_HEIGHT * 0.58f - sw * 0.10f,
                         p->pos.z + r.z * 0.42f + f.z * (0.20f + sw * 0.45f) };
        DrawCube(hand, 0.08f, 0.85f - sw * 0.45f, 0.08f,
                 (Color){ (unsigned char)(200 * tint.r / 255),
                          (unsigned char)(202 * tint.g / 255),
                          (unsigned char)(212 * tint.b / 255), 255 });
    }
}

float PlayerAttackDamage(const Player *p)
{
    float base = (p->weapon != ITEM_NONE) ? ITEMS[p->weapon].power : 6.0f;
    return base * (1.0f + (float)p->skillMelee * 0.03f);
}

float PlayerArmorValue(const Player *p)
{
    return (p->armor != ITEM_NONE) ? ITEMS[p->armor].power : 0.0f;
}

void PlayerTakeDamage(Player *p, float dmg)
{
    float reduced = dmg - PlayerArmorValue(p) * 0.6f;
    if (reduced < dmg * 0.25f) reduced = dmg * 0.25f;   /* l'armatura non annulla */

    /* Parare costa vigore in proporzione al colpo assorbito. */
    if (p->blocking) {
        p->sta -= reduced * 0.35f;
        if (p->sta < 0.0f) p->sta = 0.0f;
        reduced *= BLOCK_DMG_MUL;
    }
    p->hp -= reduced;
    p->hurtFlash = 0.35f;
    if (p->hp < 0.0f) p->hp = 0.0f;
}

void PlayerAddXP(Player *p, int amount)
{
    p->xp += amount;
    while (p->xp >= p->xpNext) {
        p->xp -= p->xpNext;
        p->level++;
        p->xpNext = 100 + p->level * 60;
        p->maxHp  += 12.0f;  p->hp  = p->maxHp;
        p->maxSta +=  6.0f;  p->sta = p->maxSta;
        p->maxMp  +=  8.0f;  p->mp  = p->maxMp;
        p->skillMelee++;
        p->skillMagic++;
    }
}

bool PlayerUseItem(Player *p, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= MAX_INVENTORY) return false;
    int id = p->inv[slotIndex].id;
    if (id == ITEM_NONE) return false;

    switch (ITEMS[id].kind) {
        case IK_WEAPON: p->weapon = id; return true;
        case IK_ARMOR:  p->armor  = id; return true;
        case IK_POTION:
            if (id == ItemFind("potion_health")) {
                p->hp = fminf(p->maxHp, p->hp + ITEMS[id].power);
            } else {
                p->mp = fminf(p->maxMp, p->mp + ITEMS[id].power);
            }
            InvRemove(p->inv, id, 1);
            return true;
        case IK_FOOD:
            p->hp  = fminf(p->maxHp,  p->hp  + ITEMS[id].power);
            p->sta = fminf(p->maxSta, p->sta + 20.0f);
            InvRemove(p->inv, id, 1);
            return true;
        default: return false;
    }
}

void PlayerUpdate(Player *p, World *w, float dt, bool controlsEnabled)
{
    /* ---- Rotazione della visuale ------------------------------------- */
    if (controlsEnabled) {
        Vector2 md = GetMouseDelta();
        p->yaw   -= md.x * MOUSE_SENS;
        p->pitch -= md.y * MOUSE_SENS;
        if (p->pitch >  1.50f) p->pitch =  1.50f;
        if (p->pitch < -1.50f) p->pitch = -1.50f;
    }

    /* ---- Visuale: prima o terza persona ------------------------------- */
    if (controlsEnabled) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (p->camMode == CAM_FIRST) {
                /* rotellina indietro: si esce dalla soggettiva */
                if (wheel < 0.0f) { p->camMode = CAM_THIRD; p->camDist = CAM_DIST_MIN; }
            } else {
                p->camDist -= wheel * CAM_ZOOM_STEP;
                if (p->camDist < CAM_DIST_MIN) p->camMode = CAM_FIRST;
                p->camDist = Clamp(p->camDist, CAM_DIST_MIN, CAM_DIST_MAX);
            }
        }
        if (IsKeyPressed(KEY_F))
            p->camMode = (p->camMode == CAM_FIRST) ? CAM_THIRD : CAM_FIRST;
    }

    /* ---- Direzioni sul piano orizzontale ------------------------------ */
    Vector3 fwd   = { sinf(p->yaw), 0.0f, cosf(p->yaw) };
    Vector3 right = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };

    Vector3 wish = { 0 };
    if (controlsEnabled) {
        if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, fwd);
        if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, fwd);
        if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
        if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    }
    if (Vector3Length(wish) > 0.001f) wish = Vector3Normalize(wish);

    /* Guardia alzata: rallenta, consuma vigore e dimezza i danni in arrivo
     * (vedi PlayerTakeDamage). Non si para a mezz'aria. */
    p->blocking = controlsEnabled && IsKeyDown(KEY_LEFT_CONTROL) &&
                  p->onGround && p->sta > 2.0f;
    if (p->blocking) p->sta -= BLOCK_STA_DRAIN * dt;

    p->sprinting = controlsEnabled && IsKeyDown(KEY_LEFT_SHIFT) && !p->blocking &&
                   p->sta > 1.0f && Vector3Length(wish) > 0.1f;

    float speed = p->sprinting ? PLAYER_RUN : PLAYER_WALK;
    if (p->inWater) speed *= 0.6f;
    if (p->blocking) speed *= BLOCK_SPEED_MUL;

    p->moveSpeed = Vector3Length(wish) * speed;

    /* Stamina: si consuma correndo, si rigenera fermi. */
    if (p->sprinting) p->sta -= 16.0f * dt;
    else              p->sta += 12.0f * dt;
    p->sta = Clamp(p->sta, 0.0f, p->maxSta);

    /* Magicka e vita rigenerano lentamente - ma non da morti, altrimenti hp
     * risale sopra zero e l'animazione di morte non parte mai. */
    p->mp = fminf(p->maxMp, p->mp + 3.5f * dt);
    if (p->hp > 0.0f) p->hp = fminf(p->maxHp, p->hp + 0.6f * dt);

    /* ---- Integrazione del movimento ----------------------------------- */
    Vector3 next = p->pos;
    next.x += wish.x * speed * dt;
    next.z += wish.z * speed * dt;

    /* Le pendenze molto ripide non sono scalabili. */
    float hNow  = WorldHeight(w, p->pos.x, p->pos.z);
    float hNext = WorldHeight(w, next.x, next.z);
    if (p->onGround && (hNext - hNow) > 1.35f * speed * dt * 2.0f) {
        next.x = p->pos.x;
        next.z = p->pos.z;
    }

    p->pos.x = next.x;
    p->pos.z = next.z;

    /* Limiti del mondo. */
    p->pos.x = Clamp(p->pos.x, 2.0f, WORLD_SIZE - 2.0f);
    p->pos.z = Clamp(p->pos.z, 2.0f, WORLD_SIZE - 2.0f);

    WorldResolveCollision(w, &p->pos, PLAYER_RADIUS);

    /* ---- Gravita' e salto --------------------------------------------- */
    float ground = WorldHeight(w, p->pos.x, p->pos.z);
    p->inWater = (ground < SEA_LEVEL - 0.2f) && (p->pos.y < SEA_LEVEL);

    if (controlsEnabled && IsKeyPressed(KEY_SPACE) && p->onGround && p->sta > 10.0f) {
        p->vel.y = PLAYER_JUMP;
        p->sta  -= 10.0f;
        p->onGround = false;
    }

    p->vel.y -= GRAVITY * dt * (p->inWater ? 0.25f : 1.0f);
    p->pos.y += p->vel.y * dt;

    float floorY = fmaxf(ground, p->inWater ? SEA_LEVEL - 1.2f : ground);
    if (p->pos.y <= floorY) {
        /* Danno da caduta. */
        if (!p->onGround && p->vel.y < -18.0f && !p->inWater)
            PlayerTakeDamage(p, (-p->vel.y - 18.0f) * 3.0f);
        p->pos.y = floorY;
        p->vel.y = 0.0f;
        p->onGround = true;
    } else {
        p->onGround = false;
    }

    /* ---- Timer e animazioni ------------------------------------------- */
    if (p->attackCd  > 0.0f) p->attackCd  -= dt;
    if (p->castCd    > 0.0f) p->castCd    -= dt;
    if (p->hurtFlash > 0.0f) p->hurtFlash -= dt;
    if (p->swing     > 0.0f) p->swing     -= dt * 2.6f;

    if (p->onGround && Vector3Length(wish) > 0.1f)
        p->bobPhase += dt * (p->sprinting ? 12.0f : 7.5f);

    PlayerUpdateAnimation(p, dt);
}
