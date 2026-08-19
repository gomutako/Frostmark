#include "entity.h"
#include "noise.h"
#include "raymath.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

const char *EntityTypeName(EntityType t)
{
    switch (t) {
        case ENT_VILLAGER: return "Popolano";
        case ENT_GUARD:    return "Guardia";
        case ENT_MERCHANT: return "Mercante";
        case ENT_ELDER:    return "Anziano";
        case ENT_WOLF:     return "Lupo";
        case ENT_BANDIT:   return "Bandito";
        case ENT_REVENANT: return "Redivivo";
        case ENT_BOSS:     return "Vald il Sepolto";
        default:           return "?";
    }
}

Color EntityColor(EntityType t)
{
    switch (t) {
        case ENT_VILLAGER: return (Color){ 172, 148, 112, 255 };
        case ENT_GUARD:    return (Color){ 108, 122, 156, 255 };
        case ENT_MERCHANT: return (Color){ 168, 118, 160, 255 };
        case ENT_ELDER:    return (Color){ 200, 196, 180, 255 };
        case ENT_WOLF:     return (Color){  98,  96,  94, 255 };
        case ENT_BANDIT:   return (Color){ 132,  84,  64, 255 };
        case ENT_REVENANT: return (Color){  92, 130, 118, 255 };
        case ENT_BOSS:     return (Color){ 150,  70,  70, 255 };
        default:           return GRAY;
    }
}

Entity *EntitySpawn(Entity *ents, EntityType type, Vector3 pos, World *w)
{
    int slot = -1;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (!ents[i].active) { slot = i; break; }
    if (slot < 0) return NULL;

    Entity *e = &ents[slot];
    memset(e, 0, sizeof(Entity));
    e->active = true;
    e->type   = type;
    e->pos    = pos;
    e->home   = pos;
    e->pos.y  = WorldHeight(w, pos.x, pos.z);
    e->state  = AI_IDLE;
    e->radius = 0.45f;
    e->height = 1.8f;
    e->dropItem = ITEM_NONE;
    e->townIndex = -1;
    TextCopy(e->name, EntityTypeName(type));

    switch (type) {
        case ENT_WOLF:
            e->maxHp = 45.0f;  e->damage = 9.0f;  e->speed = 6.2f;
            e->attackRange = 1.9f; e->hostile = true;
            e->xpReward = 28; e->goldReward = 0; e->dropItem = ITEM_WOLF_PELT;
            e->radius = 0.55f; e->height = 1.0f;
            break;
        case ENT_BANDIT:
            e->maxHp = 78.0f;  e->damage = 15.0f; e->speed = 4.6f;
            e->attackRange = 2.2f; e->hostile = true;
            e->xpReward = 46; e->goldReward = 35; e->dropItem = ITEM_BANDIT_RING;
            break;
        case ENT_REVENANT:
            e->maxHp = 110.0f; e->damage = 20.0f; e->speed = 3.4f;
            e->attackRange = 2.3f; e->hostile = true;
            e->xpReward = 65; e->goldReward = 20; e->dropItem = ITEM_BONE_DUST;
            break;
        case ENT_BOSS:
            e->maxHp = 420.0f; e->damage = 32.0f; e->speed = 4.2f;
            e->attackRange = 2.8f; e->hostile = true; e->persistent = true;
            e->xpReward = 400; e->goldReward = 600; e->dropItem = ITEM_ANCIENT_BLADE;
            e->radius = 0.75f; e->height = 2.5f;
            break;
        default:  /* NPC pacifici */
            e->maxHp = 60.0f; e->damage = 0.0f; e->speed = 1.8f;
            e->attackRange = 0.0f; e->hostile = false; e->persistent = true;
            break;
    }
    if (type == ENT_GUARD) { e->maxHp = 140.0f; e->damage = 22.0f; e->attackRange = 2.2f; }
    e->hp = e->maxHp;
    return e;
}

/* ------------------------------------------------------------------------ */
/*  IA                                                                       */
/* ------------------------------------------------------------------------ */

static void MoveTowards(Entity *e, World *w, Vector3 target, float dt)
{
    Vector3 d = Vector3Subtract(target, e->pos);
    d.y = 0.0f;
    float len = Vector3Length(d);
    if (len < 0.05f) return;
    d = Vector3Scale(d, 1.0f / len);

    e->yaw = atan2f(d.x, d.z);
    e->pos.x += d.x * e->speed * dt;
    e->pos.z += d.z * e->speed * dt;

    e->pos.x = Clamp(e->pos.x, 2.0f, WORLD_SIZE - 2.0f);
    e->pos.z = Clamp(e->pos.z, 2.0f, WORLD_SIZE - 2.0f);

    WorldResolveCollision(w, &e->pos, e->radius);
    e->pos.y = WorldHeight(w, e->pos.x, e->pos.z);
}

void EntitiesUpdate(Entity *ents, World *w, Player *p, float dt)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active) continue;

        if (e->attackCd   > 0.0f) e->attackCd   -= dt;
        if (e->stateTimer > 0.0f) e->stateTimer -= dt;

        if (e->state == AI_DEAD) continue;

        if (e->hp <= 0.0f) {
            e->state = AI_DEAD;
            e->justDied = true;          /* il game loop assegna XP e bottino */
            continue;
        }

        float dist = Vector3Distance(e->pos, p->pos);

        if (!e->hostile) {
            /* NPC pacifici: passeggiano intorno a casa e guardano il giocatore. */
            if (dist < 6.0f) {
                Vector3 d = Vector3Subtract(p->pos, e->pos);
                e->yaw = atan2f(d.x, d.z);
                e->state = AI_IDLE;
            } else if (e->stateTimer <= 0.0f) {
                e->state = (e->state == AI_WANDER) ? AI_IDLE : AI_WANDER;
                e->stateTimer = 2.0f + NoiseHash01((unsigned int)i, (int)(e->pos.x), 7) * 3.0f;
                if (e->state == AI_WANDER) {
                    float a = NoiseHash01((unsigned int)i * 13u, (int)GetTime(), 3) * 2.0f * PI;
                    e->home.x = e->home.x;   /* la "casa" resta il centro */
                    e->vel.x = cosf(a); e->vel.z = sinf(a);
                }
            }
            if (e->state == AI_WANDER) {
                Vector3 t = { e->home.x + e->vel.x * 8.0f, 0.0f, e->home.z + e->vel.z * 8.0f };
                MoveTowards(e, w, t, dt);
            } else {
                e->pos.y = WorldHeight(w, e->pos.x, e->pos.z);
            }
            continue;
        }

        /* --- Nemici: macchina a stati IDLE -> CHASE -> ATTACK ----------- */
        float aggro = (e->type == ENT_BOSS) ? 34.0f : 22.0f;

        switch (e->state) {
            case AI_IDLE:
            case AI_WANDER:
                if (dist < aggro) { e->state = AI_CHASE; break; }
                if (e->stateTimer <= 0.0f) {
                    e->stateTimer = 2.5f + NoiseHash01((unsigned)i, (int)GetTime(), 5) * 3.0f;
                    float a = NoiseHash01((unsigned)i * 7u, (int)GetTime(), 9) * 2.0f * PI;
                    e->vel.x = cosf(a); e->vel.z = sinf(a);
                    e->state = AI_WANDER;
                }
                if (e->state == AI_WANDER) {
                    Vector3 t = { e->pos.x + e->vel.x * 5.0f, 0.0f, e->pos.z + e->vel.z * 5.0f };
                    /* non allontanarsi troppo dal punto di comparsa */
                    if (Vector3Distance(e->pos, e->home) > 60.0f) t = e->home;
                    MoveTowards(e, w, t, dt);
                }
                break;

            case AI_CHASE:
                if (dist > aggro * 1.8f) { e->state = AI_IDLE; e->stateTimer = 0.0f; break; }
                if (dist <= e->attackRange) { e->state = AI_ATTACK; break; }
                MoveTowards(e, w, p->pos, dt);
                break;

            case AI_ATTACK: {
                Vector3 d = Vector3Subtract(p->pos, e->pos);
                e->yaw = atan2f(d.x, d.z);
                if (dist > e->attackRange * 1.25f) { e->state = AI_CHASE; break; }
                if (e->attackCd <= 0.0f) {
                    PlayerTakeDamage(p, e->damage);
                    e->attackCd = (e->type == ENT_WOLF) ? 1.1f : 1.6f;
                }
            } break;

            default: break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*  SPAWNER DINAMICO                                                        */
/* ------------------------------------------------------------------------ */

void EntitiesPopulate(Entity *ents, World *w, Player *p, float dt)
{
    static float timer = 0.0f;
    timer -= dt;

    /* Rimuove i corpi e i nemici troppo lontani (i "persistent" restano). */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active || e->persistent) continue;
        float d = Vector3Distance(e->pos, p->pos);
        if (d > 420.0f) e->active = false;
        if (e->state == AI_DEAD) {
            e->stateTimer -= dt;
            if (e->stateTimer < -25.0f) e->active = false;
        }
    }

    if (timer > 0.0f) return;
    timer = 1.5f;

    int nearby = 0;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (ents[i].active && ents[i].hostile && ents[i].state != AI_DEAD &&
            Vector3Distance(ents[i].pos, p->pos) < 300.0f) nearby++;

    int target = 12;
    if (nearby >= target) return;

    /* Cerca un punto valido: terra emersa, lontano dal giocatore e dai paesi */
    for (int tries = 0; tries < 24; tries++) {
        float a = NoiseHash01((unsigned int)GetTime() * 977u, tries, 1) * 2.0f * PI;
        float r = 90.0f + NoiseHash01((unsigned int)GetTime() * 131u, tries, 2) * 140.0f;
        float x = p->pos.x + cosf(a) * r;
        float z = p->pos.z + sinf(a) * r;
        if (x < 8.0f || z < 8.0f || x > WORLD_SIZE - 8.0f || z > WORLD_SIZE - 8.0f) continue;

        float h = WorldHeight(w, x, z);
        if (h < SEA_LEVEL + 1.5f) continue;

        bool inTown = false;
        for (int t = 0; t < w->townCount; t++)
            if (Vector3Distance((Vector3){x, h, z}, w->towns[t].pos) < w->towns[t].radius + 45.0f)
                inTown = true;
        if (inTown) continue;

        Biome b = WorldBiomeAt(w, x, z);
        float roll = NoiseHash01((unsigned int)GetTime() * 313u, tries, 3);
        EntityType t;
        if (b == BIOME_FOREST)                    t = (roll < 0.7f) ? ENT_WOLF : ENT_BANDIT;
        else if (b == BIOME_MOUNTAIN || b == BIOME_SNOW) t = (roll < 0.5f) ? ENT_REVENANT : ENT_WOLF;
        else                                      t = (roll < 0.55f) ? ENT_WOLF : ENT_BANDIT;

        EntitySpawn(ents, t, (Vector3){ x, h, z }, w);
        return;
    }
}

/* ------------------------------------------------------------------------ */
/*  DISEGNO                                                                 */
/* ------------------------------------------------------------------------ */

void EntitiesDraw(Entity *ents, World *w, Camera3D cam, Color tint)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active) continue;
        float dist = Vector3Distance(e->pos, cam.position);
        if (dist > 260.0f) continue;

        Color c = EntityColor(e->type);
        c = (Color){ (unsigned char)(c.r * tint.r / 255),
                     (unsigned char)(c.g * tint.g / 255),
                     (unsigned char)(c.b * tint.b / 255), 255 };

        if (e->state == AI_DEAD) {
            /* Cadavere: capsula sdraiata. */
            Vector3 a = { e->pos.x - 0.5f, e->pos.y + 0.25f, e->pos.z };
            Vector3 b = { e->pos.x + 0.5f, e->pos.y + 0.25f, e->pos.z };
            DrawCapsule(a, b, e->radius * 0.8f, 8, 6, Fade(c, 0.75f));
            continue;
        }

        if (e->type == ENT_WOLF) {
            /* Quadrupede: corpo orizzontale + testa + 4 zampe stilizzate. */
            Vector3 f = { sinf(e->yaw), 0.0f, cosf(e->yaw) };
            Vector3 a = { e->pos.x - f.x * 0.55f, e->pos.y + 0.62f, e->pos.z - f.z * 0.55f };
            Vector3 b = { e->pos.x + f.x * 0.45f, e->pos.y + 0.66f, e->pos.z + f.z * 0.45f };
            DrawCapsule(a, b, 0.34f, 8, 6, c);
            Vector3 head = { e->pos.x + f.x * 0.85f, e->pos.y + 0.72f, e->pos.z + f.z * 0.85f };
            DrawSphere(head, 0.26f, c);
            DrawSphere((Vector3){head.x + f.x*0.2f, head.y+0.05f, head.z + f.z*0.2f}, 0.09f,
                       (Color){ 220, 200, 60, 255 });
        } else {
            /* Bipede: capsula corpo + sfera testa + arma se ostile. */
            Vector3 a = { e->pos.x, e->pos.y + 0.55f, e->pos.z };
            Vector3 b = { e->pos.x, e->pos.y + e->height - 0.35f, e->pos.z };
            DrawCapsule(a, b, e->radius, 10, 6, c);
            Vector3 head = { e->pos.x, e->pos.y + e->height - 0.12f, e->pos.z };
            DrawSphere(head, 0.26f, (Color){ 216, 186, 152, 255 });

            if (e->hostile) {
                Vector3 f = { sinf(e->yaw), 0.0f, cosf(e->yaw) };
                Vector3 r = { -cosf(e->yaw), 0.0f, sinf(e->yaw) };
                Vector3 hand = { e->pos.x + r.x * 0.45f + f.x * 0.25f,
                                 e->pos.y + e->height * 0.55f,
                                 e->pos.z + r.z * 0.45f + f.z * 0.25f };
                DrawCube(hand, 0.09f, 0.9f, 0.09f, (Color){ 190, 190, 200, 255 });
            }
        }

        /* Aura del boss. */
        if (e->type == ENT_BOSS) {
            DrawSphereWires((Vector3){ e->pos.x, e->pos.y + 1.2f, e->pos.z },
                            1.8f, 6, 8, Fade((Color){ 200, 60, 60, 255 }, 0.35f));
        }
        (void)w;
    }
}

Entity *EntityLookedAt(Entity *ents, Vector3 origin, Vector3 dir,
                       float maxDist, bool hostileOnly)
{
    Vector3 fwd = Vector3Normalize(dir);
    Entity *best = NULL;
    float bestScore = 0.90f;   /* soglia di "mira": ~25 gradi */

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active || e->state == AI_DEAD) continue;
        if (hostileOnly && !e->hostile) continue;
        if (!hostileOnly && e->hostile) continue;

        Vector3 mid = { e->pos.x, e->pos.y + e->height * 0.6f, e->pos.z };
        Vector3 rel = Vector3Subtract(mid, origin);
        float d = Vector3Length(rel);
        if (d > maxDist || d < 0.001f) continue;

        float dot = Vector3DotProduct(Vector3Scale(rel, 1.0f / d), fwd);
        if (dot > bestScore) { bestScore = dot; best = e; }
    }
    return best;
}

/* ------------------------------------------------------------------------ */
/*  PROIETTILI                                                              */
/* ------------------------------------------------------------------------ */

void ProjSpawn(Projectile *pr, Vector3 pos, Vector3 dir, float dmg, bool fromPlayer)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (pr[i].active) continue;
        pr[i].active     = true;
        pr[i].pos        = pos;
        pr[i].vel        = Vector3Scale(Vector3Normalize(dir), 38.0f);
        pr[i].life       = 3.0f;
        pr[i].damage     = dmg;
        pr[i].radius     = 0.35f;
        pr[i].fromPlayer = fromPlayer;
        return;
    }
}

void ProjUpdate(Projectile *pr, Entity *ents, World *w, Player *p, float dt)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *b = &pr[i];
        if (!b->active) continue;

        b->life -= dt;
        b->vel.y -= 5.0f * dt;                       /* leggera parabola */
        b->pos = Vector3Add(b->pos, Vector3Scale(b->vel, dt));

        if (b->life <= 0.0f) { b->active = false; continue; }

        if (b->pos.y <= WorldHeight(w, b->pos.x, b->pos.z)) { b->active = false; continue; }

        if (b->fromPlayer) {
            for (int k = 0; k < MAX_ENTITIES; k++) {
                Entity *e = &ents[k];
                if (!e->active || e->state == AI_DEAD || !e->hostile) continue;
                Vector3 mid = { e->pos.x, e->pos.y + e->height * 0.5f, e->pos.z };
                if (Vector3Distance(mid, b->pos) < e->radius + b->radius + 0.4f) {
                    e->hp -= b->damage;
                    if (e->state == AI_IDLE || e->state == AI_WANDER) e->state = AI_CHASE;
                    b->active = false;
                    break;
                }
            }
        } else {
            if (Vector3Distance((Vector3){p->pos.x, p->pos.y + 1.0f, p->pos.z}, b->pos) < 0.9f) {
                PlayerTakeDamage(p, b->damage);
                b->active = false;
            }
        }
    }
}

void ProjDraw(Projectile *pr, World *w)
{
    (void)w;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pr[i].active) continue;
        DrawSphere(pr[i].pos, pr[i].radius, (Color){ 255, 170, 60, 255 });
        DrawSphereWires(pr[i].pos, pr[i].radius * 1.9f, 5, 6,
                        Fade((Color){ 255, 100, 30, 255 }, 0.5f));
    }
}
