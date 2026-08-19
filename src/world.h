/* ============================================================================
 * world.h - Mondo aperto: altimetria, biomi, streaming dei chunk, oggetti.
 *
 * IDEA CHIAVE: l'altezza del terreno e' una FUNZIONE PURA di (seed, x, z).
 * Non serve tenere in memoria una mappa: la collisione, la mesh e la minimappa
 * chiamano tutte WorldHeight(). Questo rende il mondo infinitamente
 * riproducibile e il codice molto piu' semplice da capire.
 * ========================================================================== */
#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "config.h"
#include <stdbool.h>

typedef enum {
    BIOME_OCEAN, BIOME_BEACH, BIOME_PLAINS, BIOME_FOREST,
    BIOME_HILL,  BIOME_MOUNTAIN, BIOME_SNOW, BIOME_COUNT
} Biome;

typedef enum {
    PROP_TREE, PROP_PINE, PROP_ROCK, PROP_BUSH,
    PROP_HERB, PROP_HOUSE, PROP_TOWER, PROP_CRYPT, PROP_COUNT
} PropType;

/* Un oggetto sparso sul terreno (albero, sasso, casa...). */
typedef struct {
    Vector3  pos;
    float    scale;
    float    rot;      /* gradi attorno all'asse Y                */
    float    radius;   /* raggio di collisione, 0 = attraversabile */
    PropType type;
    bool     taken;    /* per gli oggetti raccoglibili (erbe)      */
} Prop;

/* Un pezzo di mondo caricato in memoria. */
typedef struct {
    int    cx, cz;
    bool   active;
    Mesh   mesh;
    Matrix xform;
    Prop   props[MAX_PROPS_PER_CHUNK];
    int    propCount;
} Chunk;

typedef struct {
    Vector3     pos;
    float       radius;
    float       baseHeight;
    const char *name;
} Town;

typedef struct {
    unsigned int seed;

    Town  towns[MAX_TOWNS];
    int   townCount;
    Vector3 cryptPos;              /* obiettivo della quest principale */

    Chunk chunks[MAX_LOADED_CHUNKS];

    /* Heightmap opzionale (assets/heightmap.png) che sostituisce il rumore. */
    Image heightmap;
    bool  useHeightmap;

    /* Risorse grafiche condivise. */
    Material terrainMat;
    Texture2D terrainTex;
    Texture2D mapTex;              /* minimappa/mappa del mondo        */
    Model  mCyl, mCone, mSphere, mCube;   /* primitive riutilizzabili  */

    /* Modelli esterni opzionali (file .glb in assets/models/), indicizzati
     * per tipo di prop: se presenti sostituiscono le primitive.
     * Vedi docs/03-asset-pubblici.md. */
    Model  extProp[PROP_COUNT];
    bool   hasExtProp[PROP_COUNT];
} World;

void    WorldInit(World *w, unsigned int seed);
void    WorldUnload(World *w);

float   WorldHeight(const World *w, float x, float z);
Vector3 WorldNormalAt(const World *w, float x, float z);
Biome   WorldBiomeAt(const World *w, float x, float z);
Color   WorldBiomeColor(Biome b);
const char *WorldBiomeName(Biome b);

void    WorldUpdateStreaming(World *w, Vector3 center);
void    WorldDrawTerrain(World *w, Camera3D cam, Color tint);
void    WorldDrawProps(World *w, Camera3D cam, Color tint);
void    WorldDrawWater(const World *w, Vector3 camPos, Color tint, float t);

/* Spinge fuori 'pos' dai prop solidi vicini (collisione a cerchi). */
void    WorldResolveCollision(World *w, Vector3 *pos, float radius);
/* Ritorna il prop raccoglibile piu' vicino (o NULL). */
Prop   *WorldNearestProp(World *w, Vector3 pos, float maxDist, PropType type);
/* Punto sicuro (terra emersa) piu' vicino a (x,z). */
Vector3 WorldSafeSpawn(const World *w, float x, float z);

#endif /* WORLD_H */
