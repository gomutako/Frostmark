#include "world.h"
#include "worldio.h"
#include "fmath.h"
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

/* Le quote e i biomi vengono dal mondo cotto: qui non si genera piu' niente.
 * WorldHeight() sopravvive come firma - la chiamano mesh, collisioni, minimappa
 * e villaggi - ma dentro e' un'interpolazione bilineare sulla griglia caricata.
 * Sui vertici della mesh, che cadono esattamente sui campioni, restituisce il
 * valore cotto: il terreno disegnato e' quello generato. */
float WorldHeight(const World *w, float x, float z)
{
    return WorldIoHeight(&w->io, x, z);
}

Vector3 WorldNormalAt(const World *w, float x, float z)
{
    /* Il passo e' quello della griglia: campionare piu' fitto non aggiunge
     * informazione, la griglia non ce l'ha, e produce normali a scalini. */
    const float e = WORLD_GRID_STEP;
    float hl = WorldHeight(w, x - e, z);
    float hr = WorldHeight(w, x + e, z);
    float hd = WorldHeight(w, x, z - e);
    float hu = WorldHeight(w, x, z + e);
    Vector3 n = { hl - hr, 2.0f * e, hd - hu };
    return Vector3Normalize(n);
}

Biome WorldBiomeAt(const World *w, float x, float z)
{
    return WorldIoBiome(&w->io, x, z);
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

/* Grana di dettaglio: moltiplica i colori dei vertici e rompe l'effetto
 * "plastica" delle superfici piatte. Nasceva dal rumore a ogni avvio; ora che il
 * rumore vive negli strumenti, la cuoce il baker in assets/world/grain.png.
 * Se manca si va avanti con una texture bianca: e' dettaglio visivo, non un dato
 * di gioco, e un mondo senza grana e' brutto, non incoerente. */
static Texture2D LoadGrainTexture(const char *worldDir)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", worldDir, WORLD_GRAIN);

    Image img;
    if (FileExists(path)) {
        img = LoadImage(path);
    } else {
        TraceLog(LOG_WARNING, "WORLD: %s manca, terreno senza grana", path);
        img = GenImageColor(4, 4, WHITE);
    }

    Texture2D t = LoadTextureFromImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    UnloadImage(img);
    return t;
}

/* Texture del terreno: se esiste assets/textures/grass.png la usa, altrimenti
 * ricade sulla grana cotta. Nota: i colori dei vertici (bioma + luce)
 * MOLTIPLICANO questa texture, quindi una texture satura scurisce il terreno. */
static Texture2D LoadTerrainTexture(const char *worldDir)
{
    const char *file = "assets/textures/grass.png";
    if (FileExists(file)) {
        Texture2D t = LoadTexture(file);
        if (t.id != 0) {
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
            TraceLog(LOG_INFO, "WORLD: texture del terreno esterna (%s)", file);
            return t;
        }
        TraceLog(LOG_WARNING, "WORLD: %s illeggibile, uso la grana cotta", file);
    }
    return LoadGrainTexture(worldDir);
}

/* ------------------------------------------------------------------------ */
/*  MODELLI ESTERNI OPZIONALI                                               */
/* ------------------------------------------------------------------------ */

/* Un modello per tipo di prop. 'scale' porta il modello alle dimensioni che
 * DrawProp() da' alle primitive corrispondenti: i valori qui sotto sono tarati
 * sul Survival Kit di Kenney (CC0, texture dentro il .glb), dove un albero e'
 * alto 1.41 unita' mentre quello procedurale arriva a 6.5 m. L'erba curativa
 * viene dal Nature Kit: e' l'unico fiore giallo, e la quest ha bisogno che si
 * distingua da un cespuglio. Con un altro pacchetto i valori vanno rifatti:
 * 'scale' = altezza voluta in metri / altezza del modello.
 * I tipi non elencati (casa e torre) restano procedurali: nei kit CC0 di
 * Kenney gli edifici medievali sono modulari - muri, tetti, angoli - e
 * andrebbero composti, non solo caricati. */
static const struct { const char *file; float scale; } gExtProp[PROP_COUNT] = {
    [PROP_TREE] = { "assets/models/tree.glb", 4.61f },  /* h 1.41 -> 6.5 m */
    [PROP_PINE] = { "assets/models/pine.glb", 3.97f },  /* h 1.71 -> 6.8 m */
    [PROP_ROCK] = { "assets/models/rock.glb", 3.54f },  /* l 0.62 -> 2.2 m */
    [PROP_BUSH] = { "assets/models/bush.glb", 2.87f },  /* l 0.49 -> 1.4 m */
    [PROP_HERB] = { "assets/models/herb.glb", 4.50f },  /* h 0.19 -> 0.9 m */
    [PROP_CRYPT]= { "assets/models/graveyard/crypt.glb", 5.0f }, /* h 1.0 -> 5 m */
};

/* --- Edifici modulari ----------------------------------------------------
 * I pezzi vengono da due kit diversi, quindi da due cartelle: ognuno porta il
 * suo Textures/colormap.png e i due file hanno lo stesso nome.
 * La cella e' l'unita' del kit: qui vale BUILD_CELL metri. */
static const char *BUILD_FILES[BUILD_PART_COUNT] = {
    [BUILD_WALL]        = "assets/models/town/wall.glb",
    [BUILD_DOOR]        = "assets/models/town/wall-door.glb",
    [BUILD_WINDOW]      = "assets/models/town/wall-window-small.glb",
    [BUILD_ROOF]        = "assets/models/town/roof-gable.glb",
    [BUILD_FLOOR]       = "assets/models/town/planks.glb",
    [BUILD_TOWER_BASE]  = "assets/models/castle/tower-square-base.glb",
    [BUILD_TOWER_MID]   = "assets/models/castle/tower-square-mid-windows.glb",
    [BUILD_TOWER_TOP]   = "assets/models/castle/tower-square-top.glb",
    [BUILD_TOWER_ROOF]  = "assets/models/castle/tower-square-top-roof.glb",
};

/* Metri per cella. 3x2 celle fanno una casa di 7,8 x 5,2 m con i muri alti
 * 2,6: le stesse dimensioni della scatola procedurale che sostituisce. */
#define BUILD_CELL   2.6f

/* Tutti o nessuno: mezza casa e' peggio di una scatola. */
static void LoadBuildParts(World *w)
{
    for (int i = 0; i < BUILD_PART_COUNT; i++)
        if (!FileExists(BUILD_FILES[i])) return;

    for (int i = 0; i < BUILD_PART_COUNT; i++) {
        w->buildPart[i] = LoadModel(BUILD_FILES[i]);
        if (w->buildPart[i].meshCount == 0) {
            TraceLog(LOG_WARNING, "WORLD: %s non caricato, edifici procedurali",
                     BUILD_FILES[i]);
            for (int k = 0; k <= i; k++) UnloadModel(w->buildPart[k]);
            return;
        }
        for (int k = 0; k < w->buildPart[i].materialCount; k++)
            SetTextureFilter(w->buildPart[i].materials[k].maps[MATERIAL_MAP_DIFFUSE].texture,
                             TEXTURE_FILTER_POINT);
    }
    w->hasBuildParts = true;
    TraceLog(LOG_INFO, "WORLD: %d pezzi per gli edifici modulari", BUILD_PART_COUNT);
}

static void LoadExtProps(World *w)
{
    for (int t = 0; t < PROP_COUNT; t++) {
        if (gExtProp[t].file == NULL || !FileExists(gExtProp[t].file)) continue;

        Model m = LoadModel(gExtProp[t].file);
        if (m.meshCount == 0) {          /* formato non supportato o file rotto */
            TraceLog(LOG_WARNING, "WORLD: %s non caricato", gExtProp[t].file);
            UnloadModel(m);
            continue;
        }
        /* L'atlante di Kenney e' una tavolozza: ogni materiale campiona una
         * cella di colore pieno larga pochi pixel. Con i mipmap, da lontano le
         * celle vicine si mescolano. Il filtro a punti lo evita - qui non ho
         * visto differenze a occhio, ma e' il campionamento giusto per una
         * tavolozza, e la distanza massima di disegno arriva a 260 m. */
        for (int i = 0; i < m.materialCount; i++)
            SetTextureFilter(m.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture,
                             TEXTURE_FILTER_POINT);

        w->extProp[t]    = m;
        w->hasExtProp[t] = true;
        TraceLog(LOG_INFO, "WORLD: modello esterno %s (%d mesh)",
                 gExtProp[t].file, m.meshCount);
    }
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
                float d = FmClamp((SEA_LEVEL - h) / 30.0f, 0.0f, 1.0f);
                c = (Color){ (unsigned char)(52 - 30 * d),
                             (unsigned char)(92 - 45 * d),
                             (unsigned char)(140 - 55 * d), 255 };
            } else {
                c = WorldBiomeColor(WorldBiomeAt(w, wx, wz));
                /* ombreggiatura per rilievo */
                float hx = WorldHeight(w, wx + 12.0f, wz);
                float sh = FmClamp(0.75f + (h - hx) * 0.05f, 0.45f, 1.25f);
                c.r = (unsigned char)FmClamp(c.r * sh, 0, 255);
                c.g = (unsigned char)FmClamp(c.g * sh, 0, 255);
                c.b = (unsigned char)FmClamp(c.b * sh, 0, 255);
            }
            ImageDrawPixel(&img, x, y, c);
        }
    }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

/* ------------------------------------------------------------------------ */
/*  PROP: vegetazione, rocce, edifici                                       */
/* ------------------------------------------------------------------------ */

/* I prop non si spargono piu' a ogni caricamento di chunk: si leggono dal mondo
 * cotto, dove sono indicizzati per chunk. Quello che prima costava 600
 * valutazioni di rumore per chunk ora e' una decodifica di 10 byte per prop. */
static void LoadChunkProps(World *w, Chunk *c)
{
    c->propCount = WorldIoChunkProps(&w->io, c->cx, c->cz,
                                     c->props, MAX_PROPS_PER_CHUNK);
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

            m.texcoords[idx * 2 + 0] = wx / TERRAIN_UV_TILE;
            m.texcoords[idx * 2 + 1] = wz / TERRAIN_UV_TILE;

            /* Colore del bioma + illuminazione diffusa pre-calcolata. */
            Color bc = WorldBiomeColor(WorldBiomeAt(w, wx, wz));
            /* le pareti ripide diventano roccia nuda */
            float rock = FmSmoothstep(0.86f, 0.62f, nrm.y);
            bc.r = (unsigned char)FmLerp(bc.r, 105.0f, rock);
            bc.g = (unsigned char)FmLerp(bc.g, 100.0f, rock);
            bc.b = (unsigned char)FmLerp(bc.b,  95.0f, rock);

            float diff = Vector3DotProduct(nrm, sun);
            if (diff < 0.0f) diff = 0.0f;
            float lit = 0.42f + 0.58f * diff;

            m.colors[idx * 4 + 0] = (unsigned char)FmClamp(bc.r * lit, 0, 255);
            m.colors[idx * 4 + 1] = (unsigned char)FmClamp(bc.g * lit, 0, 255);
            m.colors[idx * 4 + 2] = (unsigned char)FmClamp(bc.b * lit, 0, 255);
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

bool WorldInit(World *w, const char *dir)
{
    memset(w, 0, sizeof(World));

    if (!WorldIoLoad(&w->io, dir)) return false;

    /* Il mondo cotto decide: seme, villaggi e cripta si copiano da la'. */
    w->seed      = w->io.seed;
    w->townCount = w->io.townCount;
    for (int i = 0; i < w->townCount; i++) w->towns[i] = w->io.towns[i];
    w->cryptPos  = w->io.cryptPos;

    w->terrainTex = LoadTerrainTexture(dir);
    w->terrainMat = LoadMaterialDefault();
    w->terrainMat.maps[MATERIAL_MAP_DIFFUSE].texture = w->terrainTex;

    w->mapTex = MakeWorldMap(w, 320);

    /* Primitive condivise per costruire tutti i prop. */
    w->mCyl    = LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, 10));
    w->mCone   = LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, 4));
    w->mSphere = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 10));
    w->mCube   = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    /* Modelli scaricati a mano in assets/models/ (opzionali). */
    LoadExtProps(w);
    LoadBuildParts(w);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) w->chunks[i].active = false;
    return true;
}

bool WorldValidate(const char *dir)
{
    WorldIo io;
    if (!WorldIoLoad(&io, dir)) return false;
    WorldIoFree(&io);
    return true;
}

void WorldUnload(World *w)
{
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++)
        if (w->chunks[i].active) { UnloadMesh(w->chunks[i].mesh); w->chunks[i].active = false; }

    /* WorldUnload() si chiama anche su un mondo mai caricato (GameNewWorld
     * ripulisce prima di ricaricare): senza texture da scaricare non c'e' nulla
     * da fare, e passare uno zero a UnloadTexture stampa un errore. */
    if (w->terrainTex.id != 0) UnloadTexture(w->terrainTex);
    if (w->mapTex.id != 0)     UnloadTexture(w->mapTex);
    if (w->mCyl.meshCount > 0) {
        UnloadModel(w->mCyl);
        UnloadModel(w->mCone);
        UnloadModel(w->mSphere);
        UnloadModel(w->mCube);
    }
    for (int t = 0; t < PROP_COUNT; t++)
        if (w->hasExtProp[t]) { UnloadModel(w->extProp[t]); w->hasExtProp[t] = false; }
    if (w->hasBuildParts) {
        for (int i = 0; i < BUILD_PART_COUNT; i++) UnloadModel(w->buildPart[i]);
        w->hasBuildParts = false;
    }

    WorldIoFree(&w->io);
    w->terrainTex.id = 0;
    w->mapTex.id     = 0;
    w->mCyl.meshCount = 0;
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
                        LoadChunkProps(w, c);
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

/* Distanza massima di disegno per tipo. Un cespuglio a 300 m e' un pixel che
 * costa quanto una casa: ogni prop e' una o due chiamate di disegno, e con
 * ~6000 props caricati il conto misurato era 25 ms per fotogramma, cioe' tutto
 * il budget. Gli alberi restano visibili da lontano perche' danno la forma del
 * paesaggio; case e torri sono punti di riferimento e non si tagliano. */
static float PropMaxDist(int type)
{
    switch (type) {
        case PROP_HERB:
        case PROP_BUSH: return 80.0f;
        case PROP_ROCK: return 140.0f;
        case PROP_TREE:
        case PROP_PINE: return 260.0f;
        default:        return 400.0f;
    }
}

/* Oltre questa distanza di un albero si disegna solo la chioma: il tronco e'
 * meno di un pixel e costa una chiamata intera. */
#define PROP_LOD_DIST  120.0f

/* Posa un pezzo dell'edificio. Le coordinate sono in celle rispetto al centro,
 * e vengono ruotate in blocco: cosi' la ricetta si scrive su una griglia e
 * l'orientamento dell'istanza arriva dopo. */
static void PlacePart(World *w, BuildPart part, Vector3 origin, float rotDeg,
                      float lx, float ly, float lz, float localRot,
                      float cell, Vector3 scale, Color tint)
{
    float a = rotDeg * DEG2RAD, c = cosf(a), sn = sinf(a);
    float wx = lx * cell, wz = lz * cell;
    Vector3 p = { origin.x + wx * c + wz * sn,
                  origin.y + ly * cell,
                  origin.z - wx * sn + wz * c };
    DrawModelEx(w->buildPart[part], p, (Vector3){ 0.0f, 1.0f, 0.0f },
                rotDeg + localRot, scale, tint);
}

/* Casa: 3x2 celle, muri sul perimetro, una porta al centro della facciata,
 * finestre altrove, e un tetto a due falde allungato sulla profondita'.
 * I pezzi del kit stanno sul bordo +X della loro cella: per portarli sugli
 * altri lati si ruota di 90 gradi alla volta. */
static void DrawHouse(World *w, Vector3 pos, float rotDeg, float s, Color tint)
{
    const int NX = 3, NZ = 2;
    float cell = BUILD_CELL * s;
    Vector3 sc = { cell, cell, cell };

    for (int ix = 0; ix < NX; ix++) {
        float lx = (float)ix - (NX - 1) / 2.0f;
        for (int iz = 0; iz < NZ; iz++) {
            float lz = (float)iz - (NZ - 1) / 2.0f;
            PlacePart(w, BUILD_FLOOR, pos, rotDeg, lx, 0.0f, lz,   0.0f, cell, sc, tint);
            if (ix == NX - 1)
                PlacePart(w, BUILD_WALL, pos, rotDeg, lx, 0.0f, lz,   0.0f, cell, sc, tint);
            if (ix == 0)
                PlacePart(w, BUILD_WALL, pos, rotDeg, lx, 0.0f, lz, 180.0f, cell, sc, tint);
            if (iz == 0)
                PlacePart(w, (ix == NX / 2) ? BUILD_DOOR : BUILD_WINDOW,
                          pos, rotDeg, lx, 0.0f, lz,  90.0f, cell, sc, tint);
            if (iz == NZ - 1)
                PlacePart(w, BUILD_WINDOW, pos, rotDeg, lx, 0.0f, lz, 270.0f, cell, sc, tint);
        }
    }

    /* Il pezzo del tetto e' un segmento a due falde largo una cella: due
     * affiancati farebbero una valle in mezzo, quindi se ne allunga uno solo
     * sulla profondita'. */
    /* Allungandolo in profondita' la falda si appiattisce: si alza il colmo
     * per riportare la pendenza a quella del pezzo originale. */
    Vector3 roofSc = { cell, cell * 1.6f, cell * NZ };
    for (int ix = 0; ix < NX; ix++)
        PlacePart(w, BUILD_ROOF, pos, rotDeg,
                  (float)ix - (NX - 1) / 2.0f, 1.0f, 0.0f, 0.0f, cell, roofSc, tint);
}

/* Torre: i pezzi del Castle Kit si impilano, uno per unita' di altezza. */
static void DrawTower(World *w, Vector3 pos, float rotDeg, float s, Color tint)
{
    float cell = BUILD_CELL * s;
    Vector3 sc = { cell, cell, cell };
    PlacePart(w, BUILD_TOWER_BASE, pos, rotDeg, 0.0f, 0.0f, 0.0f, 0.0f, cell, sc, tint);
    PlacePart(w, BUILD_TOWER_MID,  pos, rotDeg, 0.0f, 1.0f, 0.0f, 0.0f, cell, sc, tint);
    PlacePart(w, BUILD_TOWER_MID,  pos, rotDeg, 0.0f, 2.0f, 0.0f, 0.0f, cell, sc, tint);
    PlacePart(w, BUILD_TOWER_TOP,  pos, rotDeg, 0.0f, 3.0f, 0.0f, 0.0f, cell, sc, tint);
    PlacePart(w, BUILD_TOWER_ROOF, pos, rotDeg, 0.0f, 3.3f, 0.0f, 0.0f, cell, sc, tint);
}

static void DrawProp(World *w, const Prop *p, Color tint, bool lod)
{
    const Vector3 Y = { 0.0f, 1.0f, 0.0f };
    float s = p->scale;
    Vector3 pos = p->pos;

    /* Modello esterno al posto delle primitive, se e' stato scaricato.
     * Shade(WHITE, tint) lascia passare i colori del modello e ci applica solo
     * il ciclo giorno/notte: un tint diverso da WHITE li scurirebbe due volte. */
    if (w->hasExtProp[p->type]) {
        if (p->taken) return;
        float k = s * gExtProp[p->type].scale;
        DrawModelEx(w->extProp[p->type], pos, Y, p->rot, (Vector3){ k, k, k },
                    Shade(WHITE, tint));
        return;
    }

    switch (p->type) {
        case PROP_TREE: {
            if (!lod)
                DrawModelEx(w->mCyl, pos, Y, p->rot, (Vector3){0.30f*s, 4.2f*s, 0.30f*s},
                            Shade((Color){ 92, 66, 44, 255 }, tint));
            Vector3 top = { pos.x, pos.y + 4.6f * s, pos.z };
            DrawModelEx(w->mSphere, top, Y, p->rot, (Vector3){2.1f*s, 1.9f*s, 2.1f*s},
                        Shade((Color){ 54, 96, 46, 255 }, tint));
        } break;
        case PROP_PINE: {
            if (!lod)
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
            if (w->hasBuildParts) { DrawHouse(w, pos, p->rot, s, Shade(WHITE, tint)); break; }
            Vector3 body = { pos.x, pos.y + 1.7f, pos.z };
            DrawModelEx(w->mCube, body, Y, p->rot, (Vector3){7.0f, 3.4f, 5.5f},
                        Shade((Color){ 176, 156, 126, 255 }, tint));
            Vector3 roof = { pos.x, pos.y + 3.4f, pos.z };
            DrawModelEx(w->mCone, roof, Y, p->rot + 45.0f, (Vector3){5.6f, 2.6f, 5.6f},
                        Shade((Color){ 108, 62, 48, 255 }, tint));
        } break;
        case PROP_TOWER: {
            if (w->hasBuildParts) { DrawTower(w, pos, p->rot, s, Shade(WHITE, tint)); break; }
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
    /* La direzione della camera si normalizzava una volta per prop: con 6000
     * props sono 6000 radici quadrate buttate. Si calcola qui, una volta. */
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float lodD2 = PROP_LOD_DIST * PROP_LOD_DIST;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        for (int k = 0; k < c->propCount; k++) {
            Prop *p = &c->props[k];

            Vector3 rel = Vector3Subtract(p->pos, cam.position);
            float d2 = rel.x*rel.x + rel.y*rel.y + rel.z*rel.z;
            float md = PropMaxDist(p->type);
            if (d2 > md * md) continue;

            /* Fuori dal cono visivo, tranne quel che ci sta addosso. */
            if (d2 > 144.0f &&
                Vector3DotProduct(rel, fwd) < 0.30f * sqrtf(d2)) continue;

            DrawProp(w, p, tint, d2 > lodD2);
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
