#include "entity.h"
#include "balance.h"
#include "dataparse.h"
#include <stdio.h>
#include "fmath.h"
#include "raymath.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------ */
/*  CATALOGO DEI TIPI, CARICATO DA FILE                                     */
/* ------------------------------------------------------------------------ */

EntityDef ENTITY_TYPES[MAX_ENTITY_TYPES];
static int gTypeCount = 0;

int EntityTypeCount(void) { return gTypeCount; }

const char *EntityTypeName(int type)
{
    if (type < 0 || type >= gTypeCount) return "?";
    return ENTITY_TYPES[type].name;
}

Color EntityColor(int type)
{
    if (type < 0 || type >= gTypeCount) return GRAY;
    return ENTITY_TYPES[type].color;
}

int EntityFind(const char *id)
{
    if (id == NULL) return ENT_NONE;
    for (int i = 0; i < gTypeCount; i++)
        if (strcmp(ENTITY_TYPES[i].id, id) == 0) return i;
    return ENT_NONE;
}

static const char *const BEHAVIOUR_NAMES[] = { "pacifico", "bestia", "umanoide" };
static const char *const DRAW_NAMES[]      = { "bipede", "quadrupede" };
static const char *const BOOL_NAMES[]      = { "no", "si" };

bool EntitiesLoadTypes(const char *path)
{
    DataReader r;
    memset(ENTITY_TYPES, 0, sizeof(ENTITY_TYPES));
    gTypeCount = 0;

    if (!DataOpen(&r, path)) return false;
    int before = DataProblemCount();

    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "entita") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        if (r.id[0] == '\0') {
            DataProblem(&r, "sezione [entita] senza identificatore");
            DataSkipSection(&r);
            continue;
        }
        if (EntityFind(r.id) != ENT_NONE) {
            DataProblem(&r, "tipo \"%s\" definito due volte", r.id);
            DataSkipSection(&r);
            continue;
        }
        if (gTypeCount >= MAX_ENTITY_TYPES) {
            DataProblem(&r, "troppi tipi: il massimo e' %d", MAX_ENTITY_TYPES);
            DataSkipSection(&r);
            continue;
        }

        EntityDef *d = &ENTITY_TYPES[gTypeCount];
        memset(d, 0, sizeof(*d));
        TextCopy(d->id, r.id);
        d->behaviour   = (Behaviour)-1;
        d->maxHp       = -1.0f;
        d->damage      = -1.0f;
        d->speed       = -1.0f;
        d->attackRange = -1.0f;
        d->aggro       = -1.0f;
        d->radius      = -1.0f;
        d->height      = -1.0f;
        d->xpReward = d->goldReward = -1;
        int at = r.kindLine;

        char *key, *val;
        int tmp;
        while (DataNextField(&r, &key, &val)) {
            if      (strcmp(key, "nome") == 0)     DataAsText(&r, key, val, d->name, sizeof(d->name));
            else if (strcmp(key, "modello") == 0)  DataAsText(&r, key, val, d->model, sizeof(d->model));
            else if (strcmp(key, "bottino") == 0)  DataAsText(&r, key, val, d->loot, sizeof(d->loot));
            else if (strcmp(key, "vita") == 0)     DataAsFloat(&r, key, val, 1.0f, 100000.0f, &d->maxHp);
            else if (strcmp(key, "danno") == 0)    DataAsFloat(&r, key, val, 0.0f, 10000.0f, &d->damage);
            else if (strcmp(key, "velocita") == 0) DataAsFloat(&r, key, val, 0.0f, 100.0f, &d->speed);
            else if (strcmp(key, "portata") == 0)  DataAsFloat(&r, key, val, 0.0f, 100.0f, &d->attackRange);
            else if (strcmp(key, "aggro") == 0)    DataAsFloat(&r, key, val, 0.0f, 1000.0f, &d->aggro);
            else if (strcmp(key, "raggio") == 0)   DataAsFloat(&r, key, val, 0.05f, 10.0f, &d->radius);
            else if (strcmp(key, "altezza") == 0)  DataAsFloat(&r, key, val, 0.2f, 20.0f, &d->height);
            else if (strcmp(key, "esperienza") == 0) DataAsInt(&r, key, val, 0, 100000, &d->xpReward);
            else if (strcmp(key, "oro") == 0)      DataAsInt(&r, key, val, 0, 100000, &d->goldReward);
            else if (strcmp(key, "comportamento") == 0) {
                if (DataAsEnum(&r, key, val, BEHAVIOUR_NAMES, 3, &tmp))
                    d->behaviour = (Behaviour)tmp;
            }
            else if (strcmp(key, "disegno") == 0) {
                if (DataAsEnum(&r, key, val, DRAW_NAMES, 2, &tmp)) d->draw = (DrawKind)tmp;
            }
            else if (strcmp(key, "persistente") == 0) {
                if (DataAsEnum(&r, key, val, BOOL_NAMES, 2, &tmp)) d->persistent = (tmp != 0);
            }
            else if (strcmp(key, "aura") == 0) {
                if (DataAsEnum(&r, key, val, BOOL_NAMES, 2, &tmp)) d->aura = (tmp != 0);
            }
            else if (strcmp(key, "colore") == 0) {
                int cr = 0, cg = 0, cb = 0;
                if (sscanf(val, "%d,%d,%d", &cr, &cg, &cb) != 3 ||
                    cr < 0 || cr > 255 || cg < 0 || cg > 255 || cb < 0 || cb > 255) {
                    DataProblem(&r, "colore: atteso \"r,g,b\" con valori 0-255, trovato \"%s\"", val);
                } else {
                    d->color = (Color){ (unsigned char)cr, (unsigned char)cg,
                                        (unsigned char)cb, 255 };
                }
            }
            else DataProblem(&r, "chiave \"%s\" sconosciuta per un'entita'", key);
        }

        if (d->name[0] == '\0')        DataProblemAt(&r, at, "%s: manca \"nome\"", d->id);
        if ((int)d->behaviour < 0)     DataProblemAt(&r, at, "%s: manca \"comportamento\"", d->id);
        if (d->maxHp < 0.0f)           DataProblemAt(&r, at, "%s: manca \"vita\"", d->id);
        if (d->damage < 0.0f)          DataProblemAt(&r, at, "%s: manca \"danno\"", d->id);
        if (d->speed < 0.0f)           DataProblemAt(&r, at, "%s: manca \"velocita\"", d->id);
        if (d->attackRange < 0.0f)     DataProblemAt(&r, at, "%s: manca \"portata\"", d->id);
        if (d->aggro < 0.0f)           DataProblemAt(&r, at, "%s: manca \"aggro\"", d->id);
        if (d->radius < 0.0f)          DataProblemAt(&r, at, "%s: manca \"raggio\"", d->id);
        if (d->height < 0.0f)          DataProblemAt(&r, at, "%s: manca \"altezza\"", d->id);
        if (d->xpReward < 0)           DataProblemAt(&r, at, "%s: manca \"esperienza\"", d->id);
        if (d->goldReward < 0)         DataProblemAt(&r, at, "%s: manca \"oro\"", d->id);

        /* Il bottino cita items.txt, che e' gia' caricato: si verifica subito. */
        if (d->loot[0] != '\0' && ItemFind(d->loot) <= ITEM_NONE)
            DataProblemAt(&r, at, "%s: bottino \"%s\" non definito in items.txt",
                          d->id, d->loot);

        d->hostile = (d->behaviour != AI_PEACEFUL);
        gTypeCount++;
    }
    DataClose(&r);
    if (gTypeCount == 0) DataProblem(NULL, "%s: nessun tipo definito", path);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d tipi di personaggio da %s", gTypeCount, path);
    return ok;
}

/* ------------------------------------------------------------------------ */
/*  MODELLI ANIMATI (opzionali)                                             */
/* ------------------------------------------------------------------------ */

/* I modelli animati vengono dal campo "modello" di entities.txt. Piu' tipi
 * possono indicare lo stesso file, e in quel caso viene caricato una volta
 * sola; l'altezza per la scala e' quella del tipo. Un tipo senza modello (il
 * lupo: fra i pacchetti CC0 usati non c'e' un quadrupede animato) resta
 * disegnato con le primitive.
 *
 * I modelli sono risorse, non stato di gioco: vivono qui, statici, invece di
 * gonfiare Game o Entity. Ogni Entity porta solo la propria posa. */
static CharModel gModels[MAX_ENTITY_TYPES];
static int       gModelCount = 0;
static int       gTypeModel[MAX_ENTITY_TYPES];   /* tipo -> indice, -1 se assente */

void EntitiesLoadModels(void)
{
    EntitiesUnloadModels();
    for (int t = 0; t < MAX_ENTITY_TYPES; t++) gTypeModel[t] = -1;

    for (int t = 0; t < EntityTypeCount(); t++) {
        const char *file = ENTITY_TYPES[t].model;
        if (file[0] == '\0') continue;

        int share = -1;
        for (int k = 0; k < t; k++)                       /* stesso file? */
            if (TextIsEqual(ENTITY_TYPES[k].model, file)) share = gTypeModel[k];
        if (share >= 0) { gTypeModel[t] = share; continue; }

        if (CharModelLoad(&gModels[gModelCount], file, ENTITY_TYPES[t].height))
            gTypeModel[t] = gModelCount++;
    }
    if (gModelCount > 0)
        TraceLog(LOG_INFO, "ENTITY: %d modelli di personaggio caricati", gModelCount);
}

void EntitiesUnloadModels(void)
{
    for (int i = 0; i < gModelCount; i++) CharModelUnload(&gModels[i]);
    gModelCount = 0;
}

static const CharModel *ModelFor(int type)
{
    if (type < 0 || type >= MAX_ENTITY_TYPES) return NULL;
    int i = gTypeModel[type];
    return (i >= 0 && i < gModelCount) ? &gModels[i] : NULL;
}

/* Lo stato dell'IA e' gia' una macchina a stati: basta tradurlo. */
static CharAnim EntityPickAnim(const Entity *e)
{
    switch (e->state) {
        case AI_DEAD:   return CANIM_DEATH;
        case AI_ATTACK: return CANIM_ATTACK;
        case AI_CHASE:  return CANIM_RUN;
        case AI_WANDER: return CANIM_WALK;
        default:        return CANIM_IDLE;
    }
}

static void EntityUpdateAnim(Entity *e, float dt)
{
    const CharModel *cm = ModelFor(e->type);
    if (cm == NULL) return;

    CharAnim want = CharModelResolve(cm, EntityPickAnim(e));
    if (want != e->anim) { e->anim = want; e->animFrame = 0.0f; }

    /* Il passo segue la velocita' del tipo: un redivivo lento non deve
     * pattinare come un bandito. I riferimenti sono le andature medie. */
    float speedMul = 1.0f;
    if (want == CANIM_WALK) speedMul = e->speed / 1.8f;
    if (want == CANIM_RUN)  speedMul = e->speed / 4.5f;

    float seconds = (want == CANIM_DEATH) ? 1.30f
                  : (want == CANIM_ATTACK) ? 0.60f : 0.30f;
    e->animFrame = CharModelAdvance(cm, want, e->animFrame, dt, speedMul, seconds);
}

Entity *EntitySpawn(Entity *ents, int type, Vector3 pos, World *w)
{
    int slot = -1;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (!ents[i].active) { slot = i; break; }
    if (slot < 0) return NULL;

    if (type < 0 || type >= EntityTypeCount()) return NULL;

    Entity *e = &ents[slot];
    memset(e, 0, sizeof(Entity));
    e->active = true;
    e->type   = type;
    e->pos    = pos;
    e->home   = pos;
    e->pos.y  = WorldHeight(w, pos.x, pos.z);
    e->onGround = true;          /* nasce appoggiato, non in caduta */
    e->state  = AI_IDLE;
    e->townIndex = -1;

    /* Tutte le statistiche vengono dal catalogo caricato da entities.txt. */
    const EntityDef *d = &ENTITY_TYPES[type];
    TextCopy(e->name, d->name);
    e->maxHp       = d->maxHp;
    e->damage      = d->damage;
    e->speed       = d->speed;
    e->attackRange = d->attackRange;
    e->xpReward    = d->xpReward;
    e->goldReward  = d->goldReward;
    e->radius      = d->radius;
    e->height      = d->height;
    e->hostile     = d->hostile;
    e->persistent  = d->persistent;
    e->dropItem    = (d->loot[0] != '\0') ? ItemFind(d->loot) : ITEM_NONE;

    e->hp = e->maxHp;
    return e;
}

/* ------------------------------------------------------------------------ */
/*  IA                                                                       */
/* ------------------------------------------------------------------------ */

/* Gravita' e appoggio, con le stesse regole del giocatore (vedi PlayerUpdate):
 * in discesa ci si aggancia al terreno per il dislivello che un passo puo'
 * giustificare, altrimenti si cade. Senza, un NPC che scende un pendio
 * sobbalza, e uno che esce da un dirupo ci cammina sopra nel vuoto.
 *
 * vel.x e vel.z servono gia' alla direzione del vagabondaggio: qui si usa solo
 * vel.y, che era libero. */
static void EntityFall(Entity *e, World *w, float dt, float prevX, float prevZ)
{
    float ground  = WorldHeight(w, e->pos.x, e->pos.z);
    bool  inWater = (ground < SEA_LEVEL - 0.2f) && (e->pos.y < SEA_LEVEL);
    float floorY  = fmaxf(ground, inWater ? SEA_LEVEL - 1.2f : ground);

    if (e->onGround && e->vel.y <= 0.0f) {
        float step = hypotf(e->pos.x - prevX, e->pos.z - prevZ);
        float drop = e->pos.y - floorY;
        if (drop > 0.0f && drop <= step * STEP_DOWN_SLOPE + 0.05f) {
            e->pos.y = floorY;
            e->vel.y = 0.0f;
        }
    }

    e->vel.y -= BAL.gravity * dt * (inWater ? 0.25f : 1.0f);
    e->pos.y += e->vel.y * dt;

    if (e->pos.y <= floorY) {
        /* Nessun danno da caduta: un lupo che si butta da una rupe darebbe
         * esperienza e bottino senza che nessuno lo abbia ucciso. */
        e->pos.y    = floorY;
        e->vel.y    = 0.0f;
        e->onGround = true;
    } else {
        e->onGround = false;
    }
}

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
}

/* Il giocatore e le entita' non si attraversano piu'. Due cerchi sul piano: se
 * si sovrappongono si separano, ma non in parti uguali: la parte grossa la
 * prende l'entita'. Cosi' camminando addosso a un popolano lo si scansa, e un
 * lupo che carica non ti sposta di peso - farsi spingere in giro dai nemici e'
 * fastidioso, mentre farsi bloccare la strada e' giusto.
 *
 * Sta qui e non in PlayerUpdate perche' il giocatore non conosce le entita':
 * e' l'unico punto in cui si vedono entrambi. Gira dopo che tutti si sono
 * mossi, cosi' non dipende dall'ordine.
 *
 * I morti non fermano nessuno: sopra un cadavere ci si cammina. */
/* Le entita' fra loro. Senza, tre lupi che ti attaccano finiscono sovrapposti
 * nello stesso punto e sembrano uno solo. Qui lo spostamento si divide a meta':
 * nessuno dei due ha ragione piu' dell'altro.
 *
 * E' un ciclo su tutte le coppie, ma le entita' attive sono qualche decina e la
 * prova sul quadrato della distanza costa due moltiplicazioni: non vale la pena
 * di una griglia spaziale. */
static void EntitiesSeparate(Entity *ents, World *w)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *a = &ents[i];
        if (!a->active || a->state == AI_DEAD) continue;

        for (int j = i + 1; j < MAX_ENTITIES; j++) {
            Entity *b = &ents[j];
            if (!b->active || b->state == AI_DEAD) continue;

            if (a->pos.y > b->pos.y + b->height) continue;
            if (a->pos.y + a->height < b->pos.y) continue;

            float dx = a->pos.x - b->pos.x, dz = a->pos.z - b->pos.z;
            float d2 = dx * dx + dz * dz;
            float rr = a->radius + b->radius;
            if (d2 >= rr * rr) continue;

            float d = sqrtf(d2);
            if (d < 0.0001f) { dx = sinf(a->yaw); dz = cosf(a->yaw); d = 1.0f; }
            float push = (rr - d) / d * 0.5f;

            a->pos.x += dx * push;  a->pos.z += dz * push;
            b->pos.x -= dx * push;  b->pos.z -= dz * push;

            WorldResolveCollision(w, &a->pos, a->radius);
            WorldResolveCollision(w, &b->pos, b->radius);
        }
    }
}

void EntitiesPushPlayer(Entity *ents, World *w, Player *p)
{
    EntitiesSeparate(ents, w);

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active || e->state == AI_DEAD) continue;

        /* In verticale devono sovrapporsi: saltare sopra un lupo non conta. */
        if (p->pos.y > e->pos.y + e->height) continue;
        if (p->pos.y + BAL.bodyHeight < e->pos.y) continue;

        float dx = p->pos.x - e->pos.x, dz = p->pos.z - e->pos.z;
        float d2 = dx * dx + dz * dz;
        float rr = BAL.radius + e->radius;
        if (d2 >= rr * rr) continue;

        float d = sqrtf(d2);
        if (d < 0.0001f) {           /* esattamente sovrapposti: si sceglie una via */
            dx = sinf(e->yaw); dz = cosf(e->yaw); d = 1.0f;
        }
        float push = (rr - d) / d;

        p->pos.x += dx * push * 0.35f;
        p->pos.z += dz * push * 0.35f;
        e->pos.x -= dx * push * 0.65f;
        e->pos.z -= dz * push * 0.65f;

        /* Spinta e muri devono restare d'accordo: chi e' stato spostato torna
         * a confrontarsi con il mondo. */
        WorldResolveCollision(w, &p->pos, BAL.radius);
        WorldResolveCollision(w, &e->pos, e->radius);
    }
}

void EntitiesUpdate(Entity *ents, World *w, Player *p, float dt)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &ents[i];
        if (!e->active) continue;

        /* Da dove partiva questo passo: serve all'aggancio in discesa. */
        float prevX = e->pos.x, prevZ = e->pos.z;

        if (e->attackCd   > 0.0f) e->attackCd   -= dt;
        if (e->stateTimer > 0.0f) e->stateTimer -= dt;

        /* Prima dell'uscita anticipata: anche un morto deve finire la sua
         * animazione. */
        EntityUpdateAnim(e, dt);

        /* Anche un morto pesa: se cade mentre muore, arriva a terra. */
        if (e->state == AI_DEAD) { EntityFall(e, w, dt, prevX, prevZ); continue; }

        if (e->hp <= 0.0f) {
            e->state = AI_DEAD;
            e->justDied = true;          /* il game loop assegna XP e bottino */
            EntityFall(e, w, dt, prevX, prevZ);
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
                e->stateTimer = 2.0f + FmHash01((unsigned int)i, (int)(e->pos.x), 7) * 3.0f;
                if (e->state == AI_WANDER) {
                    float a = FmHash01((unsigned int)i * 13u, (int)GetTime(), 3) * 2.0f * PI;
                    e->home.x = e->home.x;   /* la "casa" resta il centro */
                    e->vel.x = cosf(a); e->vel.z = sinf(a);
                }
            }
            if (e->state == AI_WANDER) {
                Vector3 t = { e->home.x + e->vel.x * 8.0f, 0.0f, e->home.z + e->vel.z * 8.0f };
                MoveTowards(e, w, t, dt);
            }
            EntityFall(e, w, dt, prevX, prevZ);
            continue;
        }

        /* --- Nemici: macchina a stati IDLE -> CHASE -> ATTACK ----------- */
        float aggro = ENTITY_TYPES[e->type].aggro;

        switch (e->state) {
            case AI_IDLE:
            case AI_WANDER:
                if (dist < aggro) { e->state = AI_CHASE; break; }
                if (e->stateTimer <= 0.0f) {
                    e->stateTimer = 2.5f + FmHash01((unsigned)i, (int)GetTime(), 5) * 3.0f;
                    float a = FmHash01((unsigned)i * 7u, (int)GetTime(), 9) * 2.0f * PI;
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
                    /* le bestie colpiscono piu' spesso degli umanoidi */
                    e->attackCd = (ENTITY_TYPES[e->type].behaviour == AI_BEAST)
                                ? 1.1f : 1.6f;
                }
            } break;

            default: break;
        }

        /* La fisica chiude il passo di ogni entita', qualunque cosa abbia
         * deciso l'IA: cosi' anche un nemico fermo in aria cade. */
        EntityFall(e, w, dt, prevX, prevZ);
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
        float a = FmHash01((unsigned int)GetTime() * 977u, tries, 1) * 2.0f * PI;
        float r = 90.0f + FmHash01((unsigned int)GetTime() * 131u, tries, 2) * 140.0f;
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
        float roll = FmHash01((unsigned int)GetTime() * 313u, tries, 3);
        int wolf = EntityFind("wolf"), bandit = EntityFind("bandit");
        int revenant = EntityFind("revenant");
        int t;
        if (b == BIOME_FOREST)                    t = (roll < 0.7f) ? wolf : bandit;
        else if (b == BIOME_MOUNTAIN || b == BIOME_SNOW) t = (roll < 0.5f) ? revenant : wolf;
        else                                      t = (roll < 0.55f) ? wolf : bandit;
        if (t == ENT_NONE) continue;

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

        /* Modello animato, se per questo tipo esiste e siamo abbastanza
         * vicini: oltre NPC_MODEL_DIST il costo della deformazione non
         * ripaga, e si torna alle primitive. Il colore lo porta la texture,
         * quindi si applica solo la tinta del ciclo giorno/notte. */
        const CharModel *cm = ModelFor(e->type);
        if (cm != NULL && dist < NPC_MODEL_DIST) {
            CharModelDraw(cm, e->pos, e->yaw + PLAYER_MODEL_YAW * DEG2RAD,
                          e->anim, e->animFrame, tint);
            if (ENTITY_TYPES[e->type].aura)
                DrawSphereWires((Vector3){ e->pos.x, e->pos.y + 1.2f, e->pos.z },
                                1.8f, 6, 8, Fade((Color){ 200, 60, 60, 255 }, 0.35f));
            continue;
        }

        if (e->state == AI_DEAD) {
            /* Cadavere: capsula sdraiata. */
            Vector3 a = { e->pos.x - 0.5f, e->pos.y + 0.25f, e->pos.z };
            Vector3 b = { e->pos.x + 0.5f, e->pos.y + 0.25f, e->pos.z };
            DrawCapsule(a, b, e->radius * 0.8f, 8, 6, Fade(c, 0.75f));
            continue;
        }

        if (ENTITY_TYPES[e->type].draw == DRAW_QUADRUPED) {
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

        /* Aura, per i tipi che la dichiarano. */
        if (ENTITY_TYPES[e->type].aura) {
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
