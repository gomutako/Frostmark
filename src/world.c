#include "world.h"
#include "noise.h"
#include "raymath.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Direzione del sole usata per il "baked lighting" nei colori dei vertici.
 * Lo shader di default di raylib non calcola illuminazione: la pre-calcoliamo
 * noi una volta sola quando costruiamo la mesh. Semplice ed efficace. */
static const Vector3 SUN_DIR = { 0.45f, 0.80f, 0.40f };

/* ------------------------------------------------------------------------ */
/*  ALTIMETRIA                                                              */
/* ------------------------------------------------------------------------ */

/* Altezza "grezza", senza lo spianamento dei villaggi (evita ricorsione). */
static float RawHeight(const World *w, float x, float z)
{
    if (w->useHeightmap && w->heightmap.data != NULL) {
        /* Campionamento bilineare di una heightmap esterna (PNG in scala di
         * grigi prodotta con QGIS/GDAL, Blender A.N.T. Landscape, ecc.). */
        float u = (x / WORLD_SIZE) * (float)(w->heightmap.width  - 1);
        float v = (z / WORLD_SIZE) * (float)(w->heightmap.height - 1);
        u = NoiseClamp(u, 0.0f, (float)(w->heightmap.width  - 1));
        v = NoiseClamp(v, 0.0f, (float)(w->heightmap.height - 1));
        int x0 = (int)u, z0 = (int)v;
        int x1 = (x0 + 1 < w->heightmap.width)  ? x0 + 1 : x0;
        int z1 = (z0 + 1 < w->heightmap.height) ? z0 + 1 : z0;
        float fx = u - (float)x0, fz = v - (float)z0;

        float h00 = GetImageColor(w->heightmap, x0, z0).r / 255.0f;
        float h10 = GetImageColor(w->heightmap, x1, z0).r / 255.0f;
        float h01 = GetImageColor(w->heightmap, x0, z1).r / 255.0f;
        float h11 = GetImageColor(w->heightmap, x1, z1).r / 255.0f;
        float h = NoiseLerp(NoiseLerp(h00, h10, fx), NoiseLerp(h01, h11, fx), fz);
        return h * 110.0f;
    }

    unsigned int s = w->seed;

    /* 1) Continenti: fBm a bassa frequenza, elevato a potenza per creare
     *    grandi pianure e coste nette. */
    float cont = NoiseFBM(s, x * 0.00085f, z * 0.00085f, 5, 2.0f, 0.5f);
    cont = powf(cont, 1.55f);

    /* 2) Catene montuose: rumore "ridged" mascherato dalle zone alte. */
    float mtn  = NoiseRidged(s + 77u, x * 0.0026f, z * 0.0026f, 5);
    float mask = NoiseSmoothstep(0.42f, 0.78f, cont);

    /* 3) Colline medie + dettaglio fine. */
    float hills  = NoiseFBM(s + 131u, x * 0.006f, z * 0.006f, 3, 2.0f, 0.5f);
    float detail = NoiseFBM(s + 211u, x * 0.035f, z * 0.035f, 3, 2.0f, 0.5f);

    float h = cont * 62.0f
            + mask * mtn * 90.0f
            + hills * 10.0f
            + (detail - 0.5f) * 3.5f
            - 8.0f;

    /* 4) Spiagge: appiattisce dolcemente la fascia intorno al livello del mare */
    float band = NoiseSmoothstep(SEA_LEVEL - 3.0f, SEA_LEVEL + 4.0f, h);
    h = NoiseLerp(h, NoiseLerp(SEA_LEVEL - 1.0f, h, band), 0.45f);

    return h;
}

float WorldHeight(const World *w, float x, float z)
{
    float h = RawHeight(w, x, z);

    /* I villaggi spianano il terreno: interpoliamo verso la quota del centro. */
    for (int i = 0; i < w->townCount; i++) {
        float dx = x - w->towns[i].pos.x;
        float dz = z - w->towns[i].pos.z;
        float d  = sqrtf(dx * dx + dz * dz);
        if (d < w->towns[i].radius) {
            float t = NoiseSmoothstep(w->towns[i].radius * 0.45f,
                                      w->towns[i].radius, d);
            h = NoiseLerp(w->towns[i].baseHeight, h, t);
        }
    }
    return h;
}

Vector3 WorldNormalAt(const World *w, float x, float z)
{
    const float e = 1.0f;
    float hl = WorldHeight(w, x - e, z);
    float hr = WorldHeight(w, x + e, z);
    float hd = WorldHeight(w, x, z - e);
    float hu = WorldHeight(w, x, z + e);
    Vector3 n = { hl - hr, 2.0f * e, hd - hu };
    return Vector3Normalize(n);
}

Biome WorldBiomeAt(const World *w, float x, float z)
{
    float h = WorldHeight(w, x, z);
    if (h < SEA_LEVEL)          return BIOME_OCEAN;
    if (h < SEA_LEVEL + 2.2f)   return BIOME_BEACH;
    if (h > SNOW_LEVEL)         return BIOME_SNOW;
    if (h > MOUNTAIN_LEVEL)     return BIOME_MOUNTAIN;

    float moist = NoiseFBM(w->seed + 999u, x * 0.0018f, z * 0.0018f, 3, 2.0f, 0.5f);
    if (moist > 0.52f && h < 46.0f) return BIOME_FOREST;
    if (h > 34.0f)                  return BIOME_HILL;
    return BIOME_PLAINS;
}

Color WorldBiomeColor(Biome b)
{
    switch (b) {
        case BIOME_OCEAN:    return (Color){  62,  84,  66, 255 };
        case BIOME_BEACH:    return (Color){ 196, 182, 136, 255 };
        case BIOME_PLAINS:   return (Color){  96, 132,  66, 255 };
        case BIOME_FOREST:   return (Color){  62, 100,  52, 255 };
        case BIOME_HILL:     return (Color){ 106, 116,  70, 255 };
        case BIOME_MOUNTAIN: return (Color){ 112, 108, 102, 255 };
        case BIOME_SNOW:     return (Color){ 232, 236, 242, 255 };
        default:             return GRAY;
    }
}

const char *WorldBiomeName(Biome b)
{
    static const char *names[BIOME_COUNT] = {
        "Oceano", "Spiaggia", "Pianura", "Foresta",
        "Colline", "Montagna", "Nevi perenni"
    };
    return (b < BIOME_COUNT) ? names[b] : "?";
}

Vector3 WorldSafeSpawn(const World *w, float x, float z)
{
    for (int r = 0; r < 220; r += 6) {
        for (int a = 0; a < 12; a++) {
            float ang = (float)a * (2.0f * PI / 12.0f);
            float px = x + cosf(ang) * (float)r;
            float pz = z + sinf(ang) * (float)r;
            float h  = WorldHeight(w, px, pz);
            if (h > SEA_LEVEL + 2.0f && h < MOUNTAIN_LEVEL)
                return (Vector3){ px, h, pz };
        }
    }
    return (Vector3){ x, WorldHeight(w, x, z), z };
}

/* ------------------------------------------------------------------------ */
/*  TEXTURE PROCEDURALI                                                     */
/* ------------------------------------------------------------------------ */

/* Texture di dettaglio in scala di grigi: moltiplica i colori dei vertici e
 * rompe l'effetto "plastica" delle superfici piatte. */
static Texture2D MakeGrainTexture(unsigned int seed, int size)
{
    Image img = GenImageColor(size, size, WHITE);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float n = NoiseFBM(seed, (float)x * 0.09f, (float)y * 0.09f, 4, 2.0f, 0.5f);
            unsigned char v = (unsigned char)(190 + n * 65.0f);
            ImageDrawPixel(&img, x, y, (Color){ v, v, v, 255 });
        }
    }
    Texture2D t = LoadTextureFromImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    UnloadImage(img);
    return t;
}

/* Mappa del mondo, generata una sola volta campionando WorldHeight(). */
static Texture2D MakeWorldMap(const World *w, int size)
{
    Image img = GenImageColor(size, size, BLACK);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float wx = ((float)x / (float)size) * WORLD_SIZE;
            float wz = ((float)y / (float)size) * WORLD_SIZE;
            float h  = WorldHeight(w, wx, wz);
            Color c;
            if (h < SEA_LEVEL) {
                float d = NoiseClamp((SEA_LEVEL - h) / 30.0f, 0.0f, 1.0f);
                c = (Color){ (unsigned char)(52 - 30 * d),
                             (unsigned char)(92 - 45 * d),
                             (unsigned char)(140 - 55 * d), 255 };
            } else {
                c = WorldBiomeColor(WorldBiomeAt(w, wx, wz));
                /* ombreggiatura per rilievo */
                float hx = WorldHeight(w, wx + 12.0f, wz);
                float sh = NoiseClamp(0.75f + (h - hx) * 0.05f, 0.45f, 1.25f);
                c.r = (unsigned char)NoiseClamp(c.r * sh, 0, 255);
                c.g = (unsigned char)NoiseClamp(c.g * sh, 0, 255);
                c.b = (unsigned char)NoiseClamp(c.b * sh, 0, 255);
            }
            ImageDrawPixel(&img, x, y, c);
        }
    }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

/* ------------------------------------------------------------------------ */
/*  VILLAGGI                                                                */
/* ------------------------------------------------------------------------ */

static const char *TOWN_NAMES[MAX_TOWNS] = {
    "Pietrariva", "Valdoro", "Nordhavn", "Ceppobianco", "Rocca Grigia"
};

static void PlaceTowns(World *w)
{
    w->townCount = 0;
    unsigned int s = w->seed + 4242u;

    for (int attempt = 0; attempt < 4000 && w->townCount < MAX_TOWNS; attempt++) {
        float rx = NoiseHash01(s, attempt, 11) * (WORLD_SIZE - 800.0f) + 400.0f;
        float rz = NoiseHash01(s, attempt, 23) * (WORLD_SIZE - 800.0f) + 400.0f;

        float h = RawHeight(w, rx, rz);
        if (h < SEA_LEVEL + 4.0f || h > 42.0f) continue;

        /* Serve terreno poco ripido: campiono 4 punti attorno. */
        float maxd = 0.0f;
        for (int k = 0; k < 4; k++) {
            float ang = (float)k * (PI * 0.5f);
            float d = fabsf(RawHeight(w, rx + cosf(ang) * 45.0f,
                                         rz + sinf(ang) * 45.0f) - h);
            if (d > maxd) maxd = d;
        }
        if (maxd > 9.0f) continue;

        /* Distanza minima dagli altri villaggi. */
        bool tooClose = false;
        for (int i = 0; i < w->townCount; i++) {
            float dx = rx - w->towns[i].pos.x, dz = rz - w->towns[i].pos.z;
            if (dx * dx + dz * dz < 900.0f * 900.0f) { tooClose = true; break; }
        }
        if (tooClose) continue;

        Town *t = &w->towns[w->townCount];
        t->pos        = (Vector3){ rx, h, rz };
        t->radius     = TOWN_RADIUS;
        t->baseHeight = h;
        t->name       = TOWN_NAMES[w->townCount];
        w->townCount++;
    }

    /* Fallback: se il seed e' sfortunato, forziamo un villaggio al centro. */
    if (w->townCount == 0) {
        Vector3 p = WorldSafeSpawn(w, WORLD_SIZE * 0.5f, WORLD_SIZE * 0.5f);
        w->towns[0] = (Town){ p, TOWN_RADIUS, p.y, TOWN_NAMES[0] };
        w->townCount = 1;
    }

    /* La cripta della quest principale: lontana dal primo villaggio. */
    Vector3 base = w->towns[0].pos;
    Vector3 c = WorldSafeSpawn(w, base.x + 620.0f, base.z + 540.0f);
    w->cryptPos = c;
}

/* ------------------------------------------------------------------------ */
/*  PROP: vegetazione, rocce, edifici                                       */
/* ------------------------------------------------------------------------ */

static bool InsideAnyTown(const World *w, float x, float z, float margin)
{
    for (int i = 0; i < w->townCount; i++) {
        float dx = x - w->towns[i].pos.x, dz = z - w->towns[i].pos.z;
        if (dx * dx + dz * dz < (w->towns[i].radius + margin) *
                                (w->towns[i].radius + margin)) return true;
    }
    return false;
}

static void AddProp(Chunk *c, Prop p)
{
    if (c->propCount < MAX_PROPS_PER_CHUNK) c->props[c->propCount++] = p;
}

static void ScatterProps(World *w, Chunk *c)
{
    c->propCount = 0;
    float ox = (float)c->cx * CHUNK_SIZE;
    float oz = (float)c->cz * CHUNK_SIZE;

    /* --- Vegetazione: griglia 10x10 con jitter (Poisson "povero") -------- */
    const int CELLS = 10;
    for (int gz = 0; gz < CELLS; gz++) {
        for (int gx = 0; gx < CELLS; gx++) {
            int id = c->cx * 100003 + c->cz * 7919 + gz * CELLS + gx;
            float r1 = NoiseHash01(w->seed + 1u, id, 1);
            float r2 = NoiseHash01(w->seed + 2u, id, 2);
            float r3 = NoiseHash01(w->seed + 3u, id, 3);
            float r4 = NoiseHash01(w->seed + 4u, id, 4);

            float x = ox + ((float)gx + r1) * (CHUNK_SIZE / CELLS);
            float z = oz + ((float)gz + r2) * (CHUNK_SIZE / CELLS);
            float h = WorldHeight(w, x, z);
            if (h < SEA_LEVEL + 1.0f) continue;
            if (InsideAnyTown(w, x, z, -10.0f)) continue;

            Vector3 n = WorldNormalAt(w, x, z);
            if (n.y < 0.72f) {                       /* troppo ripido: rocce */
                if (r3 < 0.25f)
                    AddProp(c, (Prop){ (Vector3){x, h, z}, 0.8f + r4 * 1.8f,
                                       r3 * 360.0f, 1.0f, PROP_ROCK, false });
                continue;
            }

            Biome b = WorldBiomeAt(w, x, z);
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
                AddProp(c, (Prop){ (Vector3){x, h, z}, 0.75f + r4 * 0.7f,
                                   r1 * 360.0f, 0.7f, tree, false });
            } else if (r3 < density + 0.10f) {
                AddProp(c, (Prop){ (Vector3){x, h, z}, 0.6f + r4 * 0.6f,
                                   r2 * 360.0f, 0.0f, PROP_BUSH, false });
            } else if (r3 < density + 0.125f && b != BIOME_SNOW && b != BIOME_OCEAN) {
                /* Erbe curative: raccoglibili (quest secondaria). */
                AddProp(c, (Prop){ (Vector3){x, h, z}, 1.0f,
                                   0.0f, 0.0f, PROP_HERB, false });
            } else if (r3 < density + 0.155f) {
                AddProp(c, (Prop){ (Vector3){x, h, z}, 0.6f + r4 * 1.4f,
                                   r1 * 360.0f, 0.9f, PROP_ROCK, false });
            }
        }
    }

    /* --- Edifici dei villaggi -------------------------------------------- */
    for (int i = 0; i < w->townCount; i++) {
        Town *t = &w->towns[i];
        int houses = 9;
        for (int k = 0; k < houses; k++) {
            float ang = (float)k * (2.0f * PI / (float)houses)
                      + NoiseHash01(w->seed + 31u, i, k) * 0.5f;
            float rad = 22.0f + NoiseHash01(w->seed + 32u, i, k) * 26.0f;
            float x = t->pos.x + cosf(ang) * rad;
            float z = t->pos.z + sinf(ang) * rad;
            if (x < ox || x >= ox + CHUNK_SIZE || z < oz || z >= oz + CHUNK_SIZE)
                continue;
            float h = WorldHeight(w, x, z);
            float rotDeg = -ang * RAD2DEG + 90.0f;
            AddProp(c, (Prop){ (Vector3){x, h, z}, 1.0f, rotDeg, 3.6f,
                               PROP_HOUSE, false });
        }
        /* Torre di guardia al centro. */
        if (t->pos.x >= ox && t->pos.x < ox + CHUNK_SIZE &&
            t->pos.z >= oz && t->pos.z < oz + CHUNK_SIZE) {
            AddProp(c, (Prop){ (Vector3){t->pos.x, t->baseHeight, t->pos.z},
                               1.0f, 0.0f, 3.0f, PROP_TOWER, false });
        }
    }

    /* --- Cripta (obiettivo della quest principale) ----------------------- */
    if (w->cryptPos.x >= ox && w->cryptPos.x < ox + CHUNK_SIZE &&
        w->cryptPos.z >= oz && w->cryptPos.z < oz + CHUNK_SIZE) {
        AddProp(c, (Prop){ w->cryptPos, 1.0f, 25.0f, 5.0f, PROP_CRYPT, false });
    }
}

/* ------------------------------------------------------------------------ */
/*  MESH DEI CHUNK                                                          */
/* ------------------------------------------------------------------------ */

static void BuildChunkMesh(World *w, Chunk *c)
{
    Mesh m = { 0 };
    int n = CHUNK_VERTS;
    m.vertexCount   = n * n;
    m.triangleCount = CHUNK_QUADS * CHUNK_QUADS * 2;

    m.vertices  = (float *)MemAlloc((unsigned int)(m.vertexCount * 3 * sizeof(float)));
    m.normals   = (float *)MemAlloc((unsigned int)(m.vertexCount * 3 * sizeof(float)));
    m.texcoords = (float *)MemAlloc((unsigned int)(m.vertexCount * 2 * sizeof(float)));
    m.colors    = (unsigned char *)MemAlloc((unsigned int)(m.vertexCount * 4));
    m.indices   = (unsigned short *)MemAlloc((unsigned int)(m.triangleCount * 3 * sizeof(unsigned short)));

    float ox = (float)c->cx * CHUNK_SIZE;
    float oz = (float)c->cz * CHUNK_SIZE;
    Vector3 sun = Vector3Normalize(SUN_DIR);

    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            int idx = j * n + i;
            float lx = (float)i * VERT_STEP;
            float lz = (float)j * VERT_STEP;
            float wx = ox + lx, wz = oz + lz;
            float h  = WorldHeight(w, wx, wz);

            m.vertices[idx * 3 + 0] = lx;
            m.vertices[idx * 3 + 1] = h;
            m.vertices[idx * 3 + 2] = lz;

            Vector3 nrm = WorldNormalAt(w, wx, wz);
            m.normals[idx * 3 + 0] = nrm.x;
            m.normals[idx * 3 + 1] = nrm.y;
            m.normals[idx * 3 + 2] = nrm.z;

            m.texcoords[idx * 2 + 0] = wx / 8.0f;
            m.texcoords[idx * 2 + 1] = wz / 8.0f;

            /* Colore del bioma + illuminazione diffusa pre-calcolata. */
            Color bc = WorldBiomeColor(WorldBiomeAt(w, wx, wz));
            /* le pareti ripide diventano roccia nuda */
            float rock = NoiseSmoothstep(0.86f, 0.62f, nrm.y);
            bc.r = (unsigned char)NoiseLerp(bc.r, 105.0f, rock);
            bc.g = (unsigned char)NoiseLerp(bc.g, 100.0f, rock);
            bc.b = (unsigned char)NoiseLerp(bc.b,  95.0f, rock);

            float diff = Vector3DotProduct(nrm, sun);
            if (diff < 0.0f) diff = 0.0f;
            float lit = 0.42f + 0.58f * diff;

            m.colors[idx * 4 + 0] = (unsigned char)NoiseClamp(bc.r * lit, 0, 255);
            m.colors[idx * 4 + 1] = (unsigned char)NoiseClamp(bc.g * lit, 0, 255);
            m.colors[idx * 4 + 2] = (unsigned char)NoiseClamp(bc.b * lit, 0, 255);
            m.colors[idx * 4 + 3] = 255;
        }
    }

    int k = 0;
    for (int j = 0; j < CHUNK_QUADS; j++) {
        for (int i = 0; i < CHUNK_QUADS; i++) {
            unsigned short a = (unsigned short)(j * n + i);
            unsigned short b = (unsigned short)(j * n + i + 1);
            unsigned short d = (unsigned short)((j + 1) * n + i);
            unsigned short e = (unsigned short)((j + 1) * n + i + 1);
            m.indices[k++] = a; m.indices[k++] = d; m.indices[k++] = b;
            m.indices[k++] = b; m.indices[k++] = d; m.indices[k++] = e;
        }
    }

    UploadMesh(&m, false);
    c->mesh  = m;
    c->xform = MatrixTranslate(ox, 0.0f, oz);
}

/* ------------------------------------------------------------------------ */
/*  CICLO DI VITA DEL MONDO                                                 */
/* ------------------------------------------------------------------------ */

void WorldInit(World *w, unsigned int seed)
{
    memset(w, 0, sizeof(World));
    w->seed = seed;

    /* Heightmap esterna opzionale: se il file esiste, sostituisce il rumore. */
    if (FileExists("assets/heightmap.png")) {
        w->heightmap = LoadImage("assets/heightmap.png");
        if (w->heightmap.data != NULL) {
            ImageFormat(&w->heightmap, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            w->useHeightmap = true;
            TraceLog(LOG_INFO, "WORLD: heightmap esterna caricata (%dx%d)",
                     w->heightmap.width, w->heightmap.height);
        }
    }

    PlaceTowns(w);

    w->terrainTex = MakeGrainTexture(seed + 55u, 256);
    w->terrainMat = LoadMaterialDefault();
    w->terrainMat.maps[MATERIAL_MAP_DIFFUSE].texture = w->terrainTex;

    w->mapTex = MakeWorldMap(w, 320);

    /* Primitive condivise per costruire tutti i prop. */
    w->mCyl    = LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, 10));
    w->mCone   = LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, 4));
    w->mSphere = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 10));
    w->mCube   = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) w->chunks[i].active = false;
}

void WorldUnload(World *w)
{
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
        if (w->chunks[i].active) { UnloadMesh(w->chunks[i].mesh); w->chunks[i].active = false; }

    UnloadTexture(w->terrainTex);
    UnloadTexture(w->mapTex);
    UnloadModel(w->mCyl);
    UnloadModel(w->mCone);
    UnloadModel(w->mSphere);
    UnloadModel(w->mCube);
    if (w->useHeightmap) UnloadImage(w->heightmap);
    /* Nota: terrainMat deriva da LoadMaterialDefault() e condivide lo shader
     * di default: non va scaricato con UnloadMaterial(). */
}

static Chunk *FindChunk(World *w, int cx, int cz)
{
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
        if (w->chunks[i].active && w->chunks[i].cx == cx && w->chunks[i].cz == cz)
            return &w->chunks[i];
    return NULL;
}

void WorldUpdateStreaming(World *w, Vector3 center)
{
    int pcx = (int)floorf(center.x / CHUNK_SIZE);
    int pcz = (int)floorf(center.z / CHUNK_SIZE);

    /* 1) Scarica i chunk troppo lontani. */
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        if (abs(c->cx - pcx) > VIEW_CHUNKS + 1 || abs(c->cz - pcz) > VIEW_CHUNKS + 1) {
            UnloadMesh(c->mesh);
            c->active = false;
        }
    }

    /* 2) Carica i mancanti, al massimo N per frame (evita micro-scatti). */
    int built = 0;
    for (int r = 0; r <= VIEW_CHUNKS && built < CHUNK_BUILDS_PER_FRAME; r++) {
        for (int dz = -r; dz <= r && built < CHUNK_BUILDS_PER_FRAME; dz++) {
            for (int dx = -r; dx <= r && built < CHUNK_BUILDS_PER_FRAME; dx++) {
                if (abs(dx) != r && abs(dz) != r) continue;   /* solo il bordo */
                int cx = pcx + dx, cz = pcz + dz;
                if (cx < 0 || cz < 0 || cx >= WORLD_CHUNKS || cz >= WORLD_CHUNKS) continue;
                if (FindChunk(w, cx, cz)) continue;

                for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
                    if (!w->chunks[i].active) {
                        Chunk *c = &w->chunks[i];
                        c->cx = cx; c->cz = cz; c->active = true;
                        BuildChunkMesh(w, c);
                        ScatterProps(w, c);
                        built++;
                        break;
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*  DISEGNO                                                                 */
/* ------------------------------------------------------------------------ */

/* Culling molto semplice: scarta cio' che sta dietro alla camera. */
static bool InView(Camera3D cam, Vector3 p, float margin, float maxDist)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 rel = Vector3Subtract(p, cam.position);
    float dist = Vector3Length(rel);
    if (dist > maxDist) return false;
    if (dist < margin)  return true;
    return Vector3DotProduct(Vector3Normalize(rel), fwd) > 0.30f;
}

void WorldDrawTerrain(World *w, Camera3D cam, Color tint)
{
    w->terrainMat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
    float maxDist = (VIEW_CHUNKS + 1) * CHUNK_SIZE * 1.6f;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        Vector3 mid = { (c->cx + 0.5f) * CHUNK_SIZE, cam.position.y,
                        (c->cz + 0.5f) * CHUNK_SIZE };
        if (!InView(cam, mid, CHUNK_SIZE * 1.5f, maxDist)) continue;
        DrawMesh(c->mesh, w->terrainMat, c->xform);
    }
}

static Color Shade(Color c, Color tint)
{
    return (Color){ (unsigned char)(c.r * tint.r / 255),
                    (unsigned char)(c.g * tint.g / 255),
                    (unsigned char)(c.b * tint.b / 255), c.a };
}

static void DrawProp(World *w, const Prop *p, Color tint)
{
    const Vector3 Y = { 0.0f, 1.0f, 0.0f };
    float s = p->scale;
    Vector3 pos = p->pos;

    switch (p->type) {
        case PROP_TREE: {
            DrawModelEx(w->mCyl, pos, Y, p->rot, (Vector3){0.30f*s, 4.2f*s, 0.30f*s},
                        Shade((Color){ 92, 66, 44, 255 }, tint));
            Vector3 top = { pos.x, pos.y + 4.6f * s, pos.z };
            DrawModelEx(w->mSphere, top, Y, p->rot, (Vector3){2.1f*s, 1.9f*s, 2.1f*s},
                        Shade((Color){ 54, 96, 46, 255 }, tint));
        } break;
        case PROP_PINE: {
            DrawModelEx(w->mCyl, pos, Y, p->rot, (Vector3){0.26f*s, 3.0f*s, 0.26f*s},
                        Shade((Color){ 80, 58, 40, 255 }, tint));
            Vector3 a = { pos.x, pos.y + 2.2f * s, pos.z };
            DrawModelEx(w->mCone, a, Y, p->rot, (Vector3){1.9f*s, 4.6f*s, 1.9f*s},
                        Shade((Color){ 38, 74, 48, 255 }, tint));
        } break;
        case PROP_ROCK:
            DrawModelEx(w->mSphere, (Vector3){pos.x, pos.y + 0.25f*s, pos.z}, Y, p->rot,
                        (Vector3){1.1f*s, 0.75f*s, 0.95f*s},
                        Shade((Color){ 120, 118, 112, 255 }, tint));
            break;
        case PROP_BUSH:
            DrawModelEx(w->mSphere, (Vector3){pos.x, pos.y + 0.4f*s, pos.z}, Y, p->rot,
                        (Vector3){0.8f*s, 0.6f*s, 0.8f*s},
                        Shade((Color){ 66, 104, 52, 255 }, tint));
            break;
        case PROP_HERB:
            if (p->taken) return;
            DrawModelEx(w->mSphere, (Vector3){pos.x, pos.y + 0.35f, pos.z}, Y, 0.0f,
                        (Vector3){0.28f, 0.45f, 0.28f},
                        Shade((Color){ 120, 220, 150, 255 }, tint));
            DrawModelEx(w->mSphere, (Vector3){pos.x, pos.y + 0.75f, pos.z}, Y, 0.0f,
                        (Vector3){0.18f, 0.18f, 0.18f},
                        (Color){ 230, 240, 130, 255 });
            break;
        case PROP_HOUSE: {
            Vector3 body = { pos.x, pos.y + 1.7f, pos.z };
            DrawModelEx(w->mCube, body, Y, p->rot, (Vector3){7.0f, 3.4f, 5.5f},
                        Shade((Color){ 176, 156, 126, 255 }, tint));
            Vector3 roof = { pos.x, pos.y + 3.4f, pos.z };
            DrawModelEx(w->mCone, roof, Y, p->rot + 45.0f, (Vector3){5.6f, 2.6f, 5.6f},
                        Shade((Color){ 108, 62, 48, 255 }, tint));
        } break;
        case PROP_TOWER: {
            DrawModelEx(w->mCyl, pos, Y, 0.0f, (Vector3){3.0f, 11.0f, 3.0f},
                        Shade((Color){ 138, 134, 128, 255 }, tint));
            Vector3 roof = { pos.x, pos.y + 11.0f, pos.z };
            DrawModelEx(w->mCone, roof, Y, 45.0f, (Vector3){3.6f, 3.4f, 3.6f},
                        Shade((Color){ 92, 58, 46, 255 }, tint));
        } break;
        case PROP_CRYPT: {
            DrawModelEx(w->mCube, (Vector3){pos.x, pos.y + 1.2f, pos.z}, Y, p->rot,
                        (Vector3){12.0f, 2.6f, 12.0f},
                        Shade((Color){ 96, 94, 90, 255 }, tint));
            DrawModelEx(w->mCube, (Vector3){pos.x, pos.y + 3.4f, pos.z}, Y, p->rot,
                        (Vector3){5.0f, 3.6f, 4.0f},
                        Shade((Color){ 70, 68, 66, 255 }, tint));
            for (int k = 0; k < 4; k++) {
                float a = (float)k * (PI * 0.5f) + 0.78f;
                Vector3 c = { pos.x + cosf(a) * 6.5f, pos.y, pos.z + sinf(a) * 6.5f };
                DrawModelEx(w->mCyl, c, Y, 0.0f, (Vector3){0.7f, 5.5f, 0.7f},
                            Shade((Color){ 110, 108, 104, 255 }, tint));
            }
        } break;
        default: break;
    }
}

void WorldDrawProps(World *w, Camera3D cam, Color tint)
{
    float maxDist = (VIEW_CHUNKS) * CHUNK_SIZE * 1.15f;
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        for (int k = 0; k < c->propCount; k++) {
            Prop *p = &c->props[k];
            if (!InView(cam, p->pos, 12.0f, maxDist)) continue;
            DrawProp(w, p, tint);
        }
    }
}

void WorldDrawWater(const World *w, Vector3 camPos, Color tint, float t)
{
    (void)w;
    float y = SEA_LEVEL + sinf(t * 0.6f) * 0.06f;
    BeginBlendMode(BLEND_ALPHA);
    DrawPlane((Vector3){ camPos.x, y, camPos.z },
              (Vector2){ 900.0f, 900.0f },
              (Color){ (unsigned char)(38 * tint.r / 255),
                       (unsigned char)(92 * tint.g / 255),
                       (unsigned char)(140 * tint.b / 255), 185 });
    EndBlendMode();
}

/* ------------------------------------------------------------------------ */
/*  INTERAZIONE                                                             */
/* ------------------------------------------------------------------------ */

void WorldResolveCollision(World *w, Vector3 *pos, float radius)
{
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        /* salta i chunk lontani */
        float cxm = (c->cx + 0.5f) * CHUNK_SIZE, czm = (c->cz + 0.5f) * CHUNK_SIZE;
        if (fabsf(pos->x - cxm) > CHUNK_SIZE || fabsf(pos->z - czm) > CHUNK_SIZE) continue;

        for (int k = 0; k < c->propCount; k++) {
            Prop *p = &c->props[k];
            if (p->radius <= 0.0f) continue;
            float dx = pos->x - p->pos.x, dz = pos->z - p->pos.z;
            float d2 = dx * dx + dz * dz;
            float rr = p->radius * p->scale + radius;
            if (d2 < rr * rr && d2 > 0.0001f) {
                float d = sqrtf(d2);
                float push = (rr - d) / d;
                pos->x += dx * push;
                pos->z += dz * push;
            }
        }
    }
}

Prop *WorldNearestProp(World *w, Vector3 pos, float maxDist, PropType type)
{
    Prop *best = NULL;
    float bestD = maxDist * maxDist;
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        for (int k = 0; k < c->propCount; k++) {
            Prop *p = &c->props[k];
            if (p->type != type || p->taken) continue;
            float dx = pos.x - p->pos.x, dy = pos.y - p->pos.y, dz = pos.z - p->pos.z;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < bestD) { bestD = d2; best = p; }
        }
    }
    return best;
}
