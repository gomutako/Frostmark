/* ============================================================================
 * worldtypes.h - Tipi del mondo condivisi fra il gioco e gli strumenti.
 *
 * Stanno in un file a parte perche' tools/baker li usa per generare il mondo e
 * non ha alcun bisogno di sapere che esistono le mesh, le texture o lo
 * streaming dei chunk (quelli sono in world.h).
 * ========================================================================== */
#ifndef WORLDTYPES_H
#define WORLDTYPES_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    BIOME_OCEAN, BIOME_BEACH, BIOME_PLAINS, BIOME_FOREST,
    BIOME_HILL,  BIOME_MOUNTAIN, BIOME_SNOW, BIOME_COUNT
} Biome;

typedef enum {
    PROP_TREE, PROP_PINE, PROP_ROCK, PROP_BUSH,
    PROP_HERB, PROP_HOUSE, PROP_TOWER, PROP_CRYPT, PROP_COUNT
} PropType;

/* Pezzi con cui si costruiscono casa e torre. Nei kit CC0 gli edifici
 * medievali sono modulari - muro, muro con porta, falda, solaio - su una
 * griglia di celle da 1 unita': un edificio e' una ricetta, non un file. */
typedef enum {
    BUILD_WALL, BUILD_DOOR, BUILD_WINDOW, BUILD_ROOF, BUILD_FLOOR, BUILD_STAIRS,
    BUILD_TOWER_BASE, BUILD_TOWER_MID, BUILD_TOWER_TOP, BUILD_TOWER_ROOF,
    BUILD_PART_COUNT
} BuildPart;

/* Un oggetto sparso sul terreno (albero, sasso, casa...). */
typedef struct {
    Vector3  pos;
    float    scale;
    float    rot;      /* gradi attorno all'asse Y                */
    float    radius;   /* raggio di collisione, 0 = attraversabile */
    PropType type;
    bool     taken;    /* stato della partita, non del mondo: sta nel salvataggio */
} Prop;

typedef struct {
    Vector3     pos;
    float       radius;
    float       baseHeight;
    /* Copia, non un puntatore alla tabella dei nomi: lo stato non deve
     * contenere puntatori dentro le definizioni, altrimenti ricaricare i dati
     * a caldo lascerebbe puntatori pendenti. */
    char        name[24];
} Town;

/* Un NPC previsto dal mondo: il tipo si risolve per identificatore, la quota
 * viene dal terreno. Vive in assets/world/spawns.txt. */
typedef struct {
    char    type[24];      /* "elder", "guard", ... : cercato con EntityFind() */
    float   x, z;
    int     townIndex;     /* villaggio di appartenenza, -1 se nessuno */
} NpcSpawn;

#endif /* WORLDTYPES_H */
