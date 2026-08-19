#include "worldgen.h"
#include "noise.h"
#include "fmath.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/*  ALTIMETRIA                                                              */
/* ------------------------------------------------------------------------ */

/* Altezza "grezza", senza lo spianamento dei villaggi (evita ricorsione). */
static float RawHeight(const WorldGen *g, float x, float z)
{
    if (g->useHeightmap) {
        /* Campionamento bilineare della heightmap: 255 livelli di grigio
         * distribuiti su 110 m, cioe' 43 cm per livello. Grossolano rispetto ai
         * 1 cm del mondo cotto, ma e' la risoluzione del PNG, non nostra. */
        float u = (x / WORLD_SIZE) * (float)(g->heightmap.width  - 1);
        float v = (z / WORLD_SIZE) * (float)(g->heightmap.height - 1);
        u = FmClamp(u, 0.0f, (float)(g->heightmap.width  - 1));
        v = FmClamp(v, 0.0f, (float)(g->heightmap.height - 1));
        int x0 = (int)u, z0 = (int)v;
        int x1 = (x0 + 1 < g->heightmap.width)  ? x0 + 1 : x0;
        int z1 = (z0 + 1 < g->heightmap.height) ? z0 + 1 : z0;
        float fx = u - (float)x0, fz = v - (float)z0;

        float h00 = GetImageColor(g->heightmap, x0, z0).r / 255.0f;
        float h10 = GetImageColor(g->heightmap, x1, z0).r / 255.0f;
        float h01 = GetImageColor(g->heightmap, x0, z1).r / 255.0f;
        float h11 = GetImageColor(g->heightmap, x1, z1).r / 255.0f;
        float h = FmLerp(FmLerp(h00, h10, fx), FmLerp(h01, h11, fx), fz);
        return h * 110.0f;
    }

    unsigned int s = g->seed;

    /* 1) Continenti: fBm a bassa frequenza, elevato a potenza per creare
     *    grandi pianure e coste nette. */
    float cont = NoiseFBM(s, x * 0.00085f, z * 0.00085f, 5, 2.0f, 0.5f);
    cont = powf(cont, 1.55f);

    /* 2) Catene montuose: rumore "ridged" mascherato dalle zone alte. */
    float mtn  = NoiseRidged(s + 77u, x * 0.0026f, z * 0.0026f, 5);
    float mask = FmSmoothstep(0.42f, 0.78f, cont);

    /* 3) Colline medie + dettaglio fine. */
    float hills  = NoiseFBM(s + 131u, x * 0.006f, z * 0.006f, 3, 2.0f, 0.5f);
    float detail = NoiseFBM(s + 211u, x * 0.035f, z * 0.035f, 3, 2.0f, 0.5f);

    float h = cont * 62.0f
            + mask * mtn * 90.0f
            + hills * 10.0f
            + (detail - 0.5f) * 3.5f
            - 8.0f;

    /* 4) Spiagge: appiattisce dolcemente la fascia intorno al livello del mare */
    float band = FmSmoothstep(SEA_LEVEL - 3.0f, SEA_LEVEL + 4.0f, h);
    h = FmLerp(h, FmLerp(SEA_LEVEL - 1.0f, h, band), 0.45f);

    return h;
}

float GenHeight(const WorldGen *g, float x, float z)
{
    float h = RawHeight(g, x, z);

    /* I villaggi spianano il terreno: interpoliamo verso la quota del centro. */
    for (int i = 0; i < g->townCount; i++) {
        float dx = x - g->towns[i].pos.x;
        float dz = z - g->towns[i].pos.z;
        float d  = sqrtf(dx * dx + dz * dz);
        if (d < g->towns[i].radius) {
            float t = FmSmoothstep(g->towns[i].radius * 0.45f,
                                   g->towns[i].radius, d);
            h = FmLerp(g->towns[i].baseHeight, h, t);
        }
    }
    return h;
}

Vector3 GenNormal(const WorldGen *g, float x, float z)
{
    const float e = 1.0f;
    float hl = GenHeight(g, x - e, z);
    float hr = GenHeight(g, x + e, z);
    float hd = GenHeight(g, x, z - e);
    float hu = GenHeight(g, x, z + e);
    Vector3 n = { hl - hr, 2.0f * e, hd - hu };
    return Vector3Normalize(n);
}

Biome GenBiome(const WorldGen *g, float x, float z)
{
    return GenBiomeAtHeight(g, x, z, GenHeight(g, x, z));
}

Biome GenBiomeAtHeight(const WorldGen *g, float x, float z, float h)
{
    if (h < SEA_LEVEL)          return BIOME_OCEAN;
    if (h < SEA_LEVEL + 2.2f)   return BIOME_BEACH;
    if (h > SNOW_LEVEL)         return BIOME_SNOW;
    if (h > MOUNTAIN_LEVEL)     return BIOME_MOUNTAIN;

    float moist = NoiseFBM(g->seed + 999u, x * 0.0018f, z * 0.0018f, 3, 2.0f, 0.5f);
    if (moist > 0.52f && h < 46.0f) return BIOME_FOREST;
    if (h > 34.0f)                  return BIOME_HILL;
    return BIOME_PLAINS;
}

Vector3 GenSafeSpawn(const WorldGen *g, float x, float z)
{
    for (int r = 0; r < 220; r += 6) {
        for (int a = 0; a < 12; a++) {
            float ang = (float)a * (2.0f * PI / 12.0f);
            float px = x + cosf(ang) * (float)r;
            float pz = z + sinf(ang) * (float)r;
            float h  = GenHeight(g, px, pz);
            if (h > SEA_LEVEL + 2.0f && h < MOUNTAIN_LEVEL)
                return (Vector3){ px, h, pz };
        }
    }
    return (Vector3){ x, GenHeight(g, x, z), z };
}

/* ------------------------------------------------------------------------ */
/*  VILLAGGI                                                                */
/* ------------------------------------------------------------------------ */

static const char *TOWN_NAMES[MAX_TOWNS] = {
    "Pietrariva", "Valdoro", "Nordhavn", "Ceppobianco", "Rocca Grigia"
};

void GenInit(WorldGen *g, unsigned int seed, const Image *heightmap)
{
    memset(g, 0, sizeof(*g));
    g->seed = seed;
    if (heightmap != NULL && heightmap->data != NULL) {
        g->heightmap    = *heightmap;
        g->useHeightmap = true;
    }

    unsigned int s = seed + 4242u;

    for (int attempt = 0; attempt < 4000 && g->townCount < MAX_TOWNS; attempt++) {
        float rx = FmHash01(s, attempt, 11) * (WORLD_SIZE - 800.0f) + 400.0f;
        float rz = FmHash01(s, attempt, 23) * (WORLD_SIZE - 800.0f) + 400.0f;

        float h = RawHeight(g, rx, rz);
        if (h < SEA_LEVEL + 4.0f || h > 42.0f) continue;

        /* Serve terreno poco ripido: campiono 4 punti attorno. */
        float maxd = 0.0f;
        for (int k = 0; k < 4; k++) {
            float ang = (float)k * (PI * 0.5f);
            float d = fabsf(RawHeight(g, rx + cosf(ang) * 45.0f,
                                         rz + sinf(ang) * 45.0f) - h);
            if (d > maxd) maxd = d;
        }
        if (maxd > 9.0f) continue;

        /* Distanza minima dagli altri villaggi. */
        bool tooClose = false;
        for (int i = 0; i < g->townCount; i++) {
            float dx = rx - g->towns[i].pos.x, dz = rz - g->towns[i].pos.z;
            if (dx * dx + dz * dz < 900.0f * 900.0f) { tooClose = true; break; }
        }
        if (tooClose) continue;

        Town *t = &g->towns[g->townCount];
        t->pos        = (Vector3){ rx, h, rz };
        t->radius     = TOWN_RADIUS;
        t->baseHeight = h;
        TextCopy(t->name, TOWN_NAMES[g->townCount]);
        g->townCount++;
    }

    /* Se il seme e' sfortunato, forziamo un villaggio al centro: un mondo senza
     * villaggi non e' giocabile, e il baker deve accorgersene qui, non il gioco. */
    if (g->townCount == 0) {
        Vector3 p = GenSafeSpawn(g, WORLD_SIZE * 0.5f, WORLD_SIZE * 0.5f);
        g->towns[0].pos        = p;
        g->towns[0].radius     = TOWN_RADIUS;
        g->towns[0].baseHeight = p.y;
        TextCopy(g->towns[0].name, TOWN_NAMES[0]);
        g->townCount = 1;
    }

    /* La cripta della quest principale: lontana dal primo villaggio. */
    Vector3 base = g->towns[0].pos;
    g->cryptPos = GenSafeSpawn(g, base.x + 620.0f, base.z + 540.0f);
}

/* ------------------------------------------------------------------------ */
/*  PROP: vegetazione, rocce, edifici                                       */
/* ------------------------------------------------------------------------ */

static bool InsideAnyTown(const WorldGen *g, float x, float z, float margin)
{
    for (int i = 0; i < g->townCount; i++) {
        float dx = x - g->towns[i].pos.x, dz = z - g->towns[i].pos.z;
        if (dx * dx + dz * dz < (g->towns[i].radius + margin) *
                                (g->towns[i].radius + margin)) return true;
    }
    return false;
}

int GenChunkProps(const WorldGen *g, int cx, int cz, Prop *out, int cap)
{
    int n = 0;
    float ox = (float)cx * CHUNK_SIZE;
    float oz = (float)cz * CHUNK_SIZE;

    #define EMIT(p) do { if (n < cap) out[n++] = (p); } while (0)

    /* --- Vegetazione: griglia 10x10 con jitter (Poisson "povero") -------- */
    const int CELLS = 10;
    for (int gz = 0; gz < CELLS; gz++) {
        for (int gx = 0; gx < CELLS; gx++) {
            int id = cx * 100003 + cz * 7919 + gz * CELLS + gx;
            float r1 = FmHash01(g->seed + 1u, id, 1);
            float r2 = FmHash01(g->seed + 2u, id, 2);
            float r3 = FmHash01(g->seed + 3u, id, 3);
            float r4 = FmHash01(g->seed + 4u, id, 4);

            float x = ox + ((float)gx + r1) * (CHUNK_SIZE / CELLS);
            float z = oz + ((float)gz + r2) * (CHUNK_SIZE / CELLS);
            float h = GenHeight(g, x, z);
            if (h < SEA_LEVEL + 1.0f) continue;
            if (InsideAnyTown(g, x, z, -10.0f)) continue;

            Vector3 nrm = GenNormal(g, x, z);
            if (nrm.y < 0.72f) {                     /* troppo ripido: rocce */
                if (r3 < 0.25f)
                    EMIT(((Prop){ (Vector3){x, h, z}, 0.8f + r4 * 1.8f,
                                  r3 * 360.0f, 1.0f, PROP_ROCK, false }));
                continue;
            }

            Biome b = GenBiome(g, x, z);
            float density = 0.0f;
            PropType tree = PROP_TREE;
            switch (b) {
                case BIOME_FOREST:   density = 0.80f; tree = (r4 < 0.45f) ? PROP_PINE : PROP_TREE; break;
                case BIOME_PLAINS:   density = 0.16f; tree = PROP_TREE; break;
                case BIOME_HILL:     density = 0.30f; tree = PROP_PINE; break;
                case BIOME_MOUNTAIN: density = 0.10f; tree = PROP_PINE; break;
                case BIOME_BEACH:    density = 0.05f; tree = PROP_BUSH; break;
                default: break;
            }

            if (r3 < density) {
                EMIT(((Prop){ (Vector3){x, h, z}, 0.75f + r4 * 0.7f,
                              r1 * 360.0f, 0.7f, tree, false }));
            } else if (r3 < density + 0.10f) {
                EMIT(((Prop){ (Vector3){x, h, z}, 0.6f + r4 * 0.6f,
                              r2 * 360.0f, 0.0f, PROP_BUSH, false }));
            } else if (r3 < density + 0.125f && b != BIOME_SNOW && b != BIOME_OCEAN) {
                /* Erbe curative: raccoglibili (quest secondaria). */
                EMIT(((Prop){ (Vector3){x, h, z}, 1.0f,
                              0.0f, 0.0f, PROP_HERB, false }));
            } else if (r3 < density + 0.155f) {
                EMIT(((Prop){ (Vector3){x, h, z}, 0.6f + r4 * 1.4f,
                              r1 * 360.0f, 0.9f, PROP_ROCK, false }));
            }
        }
    }

    /* --- Edifici dei villaggi -------------------------------------------- */
    for (int i = 0; i < g->townCount; i++) {
        const Town *t = &g->towns[i];
        int houses = 9;
        for (int k = 0; k < houses; k++) {
            float ang = (float)k * (2.0f * PI / (float)houses)
                      + FmHash01(g->seed + 31u, i, k) * 0.5f;
            float rad = 22.0f + FmHash01(g->seed + 32u, i, k) * 26.0f;
            float x = t->pos.x + cosf(ang) * rad;
            float z = t->pos.z + sinf(ang) * rad;
            if (x < ox || x >= ox + CHUNK_SIZE || z < oz || z >= oz + CHUNK_SIZE)
                continue;
            float h = GenHeight(g, x, z);
            float rotDeg = -ang * RAD2DEG + 90.0f;
            EMIT(((Prop){ (Vector3){x, h, z}, 1.0f, rotDeg, 3.6f,
                          PROP_HOUSE, false }));
        }
        /* Torre di guardia al centro. */
        if (t->pos.x >= ox && t->pos.x < ox + CHUNK_SIZE &&
            t->pos.z >= oz && t->pos.z < oz + CHUNK_SIZE) {
            EMIT(((Prop){ (Vector3){t->pos.x, t->baseHeight, t->pos.z},
                          1.0f, 0.0f, 3.0f, PROP_TOWER, false }));
        }
    }

    /* --- Cripta (obiettivo della quest principale) ----------------------- */
    if (g->cryptPos.x >= ox && g->cryptPos.x < ox + CHUNK_SIZE &&
        g->cryptPos.z >= oz && g->cryptPos.z < oz + CHUNK_SIZE) {
        EMIT(((Prop){ g->cryptPos, 1.0f, 25.0f, 5.0f, PROP_CRYPT, false }));
    }

    #undef EMIT
    return n;
}

/* ------------------------------------------------------------------------ */
/*  ORGANICO DEI VILLAGGI E PUNTO DI PARTENZA                               */
/* ------------------------------------------------------------------------ */

int GenTownNpcs(const WorldGen *g, int townIndex, NpcSpawn *out, int cap)
{
    /* Gli stessi tipi, angoli e raggi che stavano in SpawnTownNPCs(). Qui
     * diventano coordinate assolute: nel mondo cotto un NPC e' un punto, non un
     * angolo intorno a un centro, cosi' spostarlo in spawns.txt e' banale. */
    static const struct { const char *type; float ang; float rad; } roster[] = {
        { "elder",    0.4f, 14.0f },
        { "merchant", 1.9f, 16.0f },
        { "guard",    3.2f, 20.0f },
        { "guard",    5.0f, 20.0f },
        { "villager", 2.6f, 26.0f },
        { "villager", 4.4f, 24.0f },
        { "villager", 0.9f, 30.0f },
    };
    const int total = (int)(sizeof(roster) / sizeof(roster[0]));

    if (townIndex < 0 || townIndex >= g->townCount) return 0;
    const Town *t = &g->towns[townIndex];

    int n = 0;
    for (int i = 0; i < total && n < cap; i++) {
        NpcSpawn *s = &out[n++];
        TextCopy(s->type, roster[i].type);
        s->x = t->pos.x + cosf(roster[i].ang) * roster[i].rad;
        s->z = t->pos.z + sinf(roster[i].ang) * roster[i].rad;
        s->townIndex = townIndex;
    }
    return n;
}

Vector3 GenPlayerStart(const WorldGen *g)
{
    Vector3 t = g->towns[0].pos;
    return GenSafeSpawn(g, t.x + 26.0f, t.z + 26.0f);
}
