#include "player.h"
#include "balance.h"
#include "mouse.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

/* La sensibilita' sta in balance.txt: col movimento grezzo l'unita' dipende
 * dai DPI del mouse, e si tara provando.
 * Oltre questo scarto per fotogramma il dato non e' credibile: un colpo di
 * polso veloce arriva a qualche centinaio di conteggi, la spazzatura misurata
 * sotto WSLg stava fra 13.000 e 35.000. */
#define MOUSE_MAX_DELTA 2500.0f

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
        speedMul = p->moveSpeed / ((want == CANIM_RUN) ? BAL.run : BAL.walk);

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
    p->camDistActual = CAM_DIST_DEF;
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
    CharModelLoad(&p->model, PLAYER_MODEL_FILE, BAL.bodyHeight);
}

Vector3 PlayerEye(const Player *p)
{
    return (Vector3){ p->pos.x, p->pos.y + BAL.eyeHeight, p->pos.z };
}

Vector3 PlayerLookDir(const Player *p)
{
    return (Vector3){ cosf(p->pitch) * sinf(p->yaw),
                      sinf(p->pitch),
                      cosf(p->pitch) * cosf(p->yaw) };
}

void PlayerCamera(Player *p, const World *w, Camera3D *cam)
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

    /* Dentro casa la camera non puo' restare a sei metri: sarebbe fuori, a
     * inquadrare un muro. Si accorcia, e i passi sotto la fermano comunque
     * prima di attraversare una parete. */
    bool indoors = WorldInsideBuilding(w, p->pos);
    float want = indoors ? fminf(p->camDist, CAM_DIST_INDOOR) : p->camDist;

    /* Gli edifici si tagliano esatti, non a campioni: un muro e' sottile e fra
     * due campioni ci passa. Vedi WorldCameraClip(). */
    Vector3 back = { -fwd.x, -fwd.y, -fwd.z };
    want = fminf(want, WorldCameraClip(w, eye, back, want));

    float dist = want;
    for (int i = 1; i <= STEPS; i++) {
        float t = want * (float)i / (float)STEPS;
        Vector3 s = Vector3Subtract(eye, Vector3Scale(fwd, t));
        if (s.y < WorldHeight(w, s.x, s.z) + CAM_CLEARANCE) {
            dist = t - want / (float)STEPS;         /* fermati un passo prima */
            break;
        }
    }
    if (dist < 0.35f) dist = 0.35f;

    /* Rientrare deve essere immediato - un fotogramma con il muro davanti si
     * vede - ma riallontanarsi no: in un bosco fitto la camera entrerebbe e
     * uscirebbe a ogni tronco. Quindi dentro subito, fuori piano. */
    if (dist < p->camDistActual) p->camDistActual = dist;
    else p->camDistActual = fminf(dist, p->camDistActual + CAM_RETURN_SPEED * GetFrameTime());
    dist = p->camDistActual;

    /* Scostamento in alto e a destra: senza, il giocatore sta esattamente sul
     * mirino e copre quello che si sta inquadrando. Il prezzo e' che l'asse
     * della camera non coincide piu' con la direzione di mira - qui sono 3
     * gradi - ma il combattimento non ne soffre: usa PlayerLookDir(), non la
     * camera. */
    /* Lo scostamento si riduce con la distanza: a camera accostata sposterebbe
     * l'obiettivo dentro il muro che si e' appena evitato. */
    float k = (p->camDist > 0.01f) ? dist / p->camDist : 0.0f;
    Vector3 right = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };
    cam->position = Vector3Subtract(eye, Vector3Scale(fwd, dist));
    cam->position = Vector3Add(cam->position, Vector3Scale(right, CAM_SHOULDER * k));
    cam->position.y += CAM_RISE * k;

    float minY = WorldHeight(w, cam->position.x, cam->position.z) + CAM_CLEARANCE;
    if (cam->position.y < minY) cam->position.y = minY;

    cam->target = Vector3Add(eye, Vector3Scale(fwd, 4.0f));
}

/* Disegna il modello animato. Con uno scheletro si disegnano solo le mesh
 * skinnate (vedi PlayerLoadModel), altrimenti tutte. */
void PlayerDraw(const Player *p, Color tint)
{
    if (p->camMode == CAM_FIRST) return;

    /* Quando un muro costringe la camera addosso al collo, il modello coprirebbe
     * tutta la scena. Invece di farlo sparire di colpo lo si dissolve: fra
     * CAM_FADE_OUT e CAM_FADE_IN passa da invisibile a pieno, e l'occhio non
     * vede uno scatto. Il giocatore resta disegnato per ultimo fra le cose
     * solide, quindi la trasparenza non ha bisogno di ordinamenti. */
    float fade = (p->camDistActual - CAM_FADE_OUT) / (CAM_FADE_IN - CAM_FADE_OUT);
    if (fade <= 0.0f) return;
    if (fade > 1.0f) fade = 1.0f;
    unsigned char alpha = (unsigned char)(255.0f * fade);

    if (p->model.loaded) {
        Color t = { tint.r, tint.g, tint.b, alpha };
        BeginBlendMode(BLEND_ALPHA);
        CharModelDraw(&p->model, p->pos, p->yaw + PLAYER_MODEL_YAW * DEG2RAD,
                      p->anim, p->animFrame, t);
        EndBlendMode();
        return;
    }

    Color body = { (unsigned char)( 96 * tint.r / 255),
                   (unsigned char)(104 * tint.g / 255),
                   (unsigned char)(126 * tint.b / 255), alpha };
    Color skin = { (unsigned char)(216 * tint.r / 255),
                   (unsigned char)(186 * tint.g / 255),
                   (unsigned char)(152 * tint.b / 255), alpha };

    /* Stessa grammatica visiva dei bipedi in entity.c: capsula, testa, arma.
     * Il raggio e' piu' sottile di BAL.radius, che serve alle collisioni:
     * disegnarlo a 0.45 m dava un personaggio a forma di botte che copriva
     * anche la spada. */
    const float BODY_R = 0.30f;
    Vector3 f = { sinf(p->yaw), 0.0f, cosf(p->yaw) };
    Vector3 r = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };

    Vector3 a = { p->pos.x, p->pos.y + 0.45f, p->pos.z };
    Vector3 b = { p->pos.x, p->pos.y + BAL.bodyHeight - 0.55f, p->pos.z };
    DrawCapsule(a, b, BODY_R, 10, 6, body);
    DrawSphere((Vector3){ p->pos.x, p->pos.y + BAL.bodyHeight - 0.22f, p->pos.z },
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
                         p->pos.y + BAL.bodyHeight * 0.58f - sw * 0.10f,
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
        reduced *= BAL.blockDamageMul;
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
        p->xpNext = BAL.xpBase + p->level * BAL.xpPerLevel;
        p->maxHp  += BAL.hpPerLevel;  p->hp  = p->maxHp;
        p->maxSta += BAL.staPerLevel; p->sta = p->maxSta;
        p->maxMp  += BAL.mpPerLevel;  p->mp  = p->maxMp;
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

/* Input istantaneo della visuale: si legge una volta per fotogramma, non a
 * ogni passo di simulazione. Il delta del mouse e' gia' un movimento, non una
 * velocita': applicarlo dentro il ciclo a passo fisso lo moltiplicherebbe per
 * il numero di passi del fotogramma, e la visuale schizzerebbe in verticale. */
void PlayerLook(Player *p)
{
    Vector2 md = MouseLookDelta();

    /* Rete di sicurezza: un delta cosi' grande non viene da una mano - viene
     * da un driver che sbaglia (vedi GrabMouse in game.c) o dalla finestra che
     * riprende il fuoco. Si butta via invece di limitarlo, per non introdurre
     * una rotazione che il giocatore non ha chiesto. */
    if (fabsf(md.x) > MOUSE_MAX_DELTA || fabsf(md.y) > MOUSE_MAX_DELTA) return;

    p->yaw   -= md.x * BAL.mouseSens;
    p->pitch -= md.y * BAL.mouseSens;
    if (p->pitch >  1.50f) p->pitch =  1.50f;
    if (p->pitch < -1.50f) p->pitch = -1.50f;

    /* ---- Visuale: prima o terza persona ------------------------------- */
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

    /* Il salto e' un evento: si registra qui e si consuma al primo passo di
     * simulazione, altrimenti un fotogramma lento darebbe piu' spinte. */
    if (IsKeyPressed(KEY_SPACE)) p->jumpQueued = true;
}

void PlayerUpdate(Player *p, World *w, float dt, bool controlsEnabled)
{
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
    if (p->blocking) p->sta -= BAL.blockStaminaDrain * dt;

    p->sprinting = controlsEnabled && IsKeyDown(KEY_LEFT_SHIFT) && !p->blocking &&
                   p->sta > 1.0f && Vector3Length(wish) > 0.1f;

    float speed = p->sprinting ? BAL.run : BAL.walk;
    if (p->inWater) speed *= 0.6f;
    if (p->blocking) speed *= BAL.blockSpeedMul;

    p->moveSpeed = Vector3Length(wish) * speed;

    /* Stamina: si consuma correndo, si rigenera fermi. */
    if (p->sprinting) p->sta -= BAL.staminaSprint * dt;
    else              p->sta += BAL.staminaRegen * dt;
    p->sta = Clamp(p->sta, 0.0f, p->maxSta);

    /* Magicka e vita rigenerano lentamente - ma non da morti, altrimenti hp
     * risale sopra zero e l'animazione di morte non parte mai. */
    p->mp = fminf(p->maxMp, p->mp + BAL.manaRegen * dt);
    if (p->hp > 0.0f) p->hp = fminf(p->maxHp, p->hp + BAL.healthRegen * dt);

    /* ---- Integrazione del movimento ----------------------------------- */
    float prevX = p->pos.x, prevZ = p->pos.z;
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

    WorldResolveCollision(w, &p->pos, BAL.radius);

    /* ---- Gravita' e salto --------------------------------------------- */
    float ground = WorldHeight(w, p->pos.x, p->pos.z);
    p->inWater = (ground < SEA_LEVEL - 0.2f) && (p->pos.y < SEA_LEVEL);

    if (p->jumpQueued) {
        p->jumpQueued = false;
        if (controlsEnabled && p->onGround && p->sta > 10.0f) {
            p->vel.y = BAL.jump;
            p->sta  -= BAL.staminaJump;
            p->onGround = false;
        }
    }

    float floorY = fmaxf(ground, p->inWater ? SEA_LEVEL - 1.2f : ground);

    /* Dentro un edificio alto si cammina sui solai e sulle scale, non sul
     * terreno che sta sotto. Il margine e' quello di un gradino: piu' in alto
     * di cosi' non ci si arrampica. */
    float support = WorldSupportHeight(w, p->pos, STEP_UP_REACH);
    if (support > floorY) floorY = support;

    /* Aggancio al terreno in discesa. Senza, a ogni passo il suolo scende
     * sotto i piedi, il giocatore resta in aria per un fotogramma, la gravita'
     * lo riprende e atterra: un sobbalzo ritmico di pochi centimetri, e per
     * giunta 'onGround' falso quasi sempre - niente salto, niente parata.
     * Ci si incolla solo per il dislivello che un passo puo' giustificare:
     * un salto nel vuoto resta una caduta. */
    if (p->onGround && p->vel.y <= 0.0f) {
        float step = hypotf(p->pos.x - prevX, p->pos.z - prevZ);
        float drop = p->pos.y - floorY;
        if (drop > 0.0f && drop <= step * STEP_DOWN_SLOPE + 0.05f) {
            p->pos.y  = floorY;
            p->vel.y  = 0.0f;
        }
    }

    p->vel.y -= BAL.gravity * dt * (p->inWater ? 0.25f : 1.0f);
    p->pos.y += p->vel.y * dt;

    if (p->pos.y <= floorY) {
        /* Danno da caduta. */
        if (!p->onGround && p->vel.y < -BAL.fallThreshold && !p->inWater)
            PlayerTakeDamage(p, (-p->vel.y - BAL.fallThreshold) * BAL.fallFactor);
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
