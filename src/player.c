#include "player.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define MOUSE_SENS 0.0026f

void PlayerInit(Player *p, Vector3 spawn)
{
    memset(p, 0, sizeof(Player));
    p->pos   = spawn;
    p->yaw   = 0.0f;
    p->pitch = -0.05f;

    p->maxHp = 100.0f; p->hp  = p->maxHp;
    p->maxSta= 100.0f; p->sta = p->maxSta;
    p->maxMp =  60.0f; p->mp  = p->maxMp;

    p->level = 1; p->xp = 0; p->xpNext = 120; p->gold = 25;
    p->skillMelee = 5; p->skillMagic = 5;

    p->weapon = ITEM_RUSTY_SWORD;
    p->armor  = ITEM_NONE;

    InvAdd(p->inv, ITEM_RUSTY_SWORD, 1);
    InvAdd(p->inv, ITEM_POTION_HEALTH, 2);
    InvAdd(p->inv, ITEM_BREAD, 3);
}

void PlayerCamera(const Player *p, Camera3D *cam)
{
    /* Leggero "head bob" quando si cammina: costa poco, aggiunge molto. */
    float bob = sinf(p->bobPhase) * 0.055f;
    cam->position = (Vector3){ p->pos.x, p->pos.y + PLAYER_EYE + bob, p->pos.z };

    Vector3 fwd = { cosf(p->pitch) * sinf(p->yaw),
                    sinf(p->pitch),
                    cosf(p->pitch) * cosf(p->yaw) };
    cam->target = Vector3Add(cam->position, fwd);
    cam->up     = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->fovy   = 70.0f;
    cam->projection = CAMERA_PERSPECTIVE;
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
            if (id == ITEM_POTION_HEALTH) {
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

    p->sprinting = controlsEnabled && IsKeyDown(KEY_LEFT_SHIFT) &&
                   p->sta > 1.0f && Vector3Length(wish) > 0.1f;

    float speed = p->sprinting ? PLAYER_RUN : PLAYER_WALK;
    if (p->inWater) speed *= 0.6f;

    /* Stamina: si consuma correndo, si rigenera fermi. */
    if (p->sprinting) p->sta -= 16.0f * dt;
    else              p->sta += 12.0f * dt;
    p->sta = Clamp(p->sta, 0.0f, p->maxSta);

    /* Magicka e vita rigenerano lentamente. */
    p->mp = fminf(p->maxMp, p->mp + 3.5f * dt);
    p->hp = fminf(p->maxHp, p->hp + 0.6f * dt);

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
}
