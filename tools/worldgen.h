/* ============================================================================
 * worldgen.h - Generazione del mondo dal seme. Vive negli strumenti.
 *
 * E' il codice che stava in world.c fino alla fase 3 del piano (docs/05): il
 * rumore, lo spianamento dei villaggi, la scelta dei biomi e lo spargimento dei
 * prop. Il gioco non lo compila piu': legge assets/world/, che questo codice ha
 * scritto una volta tramite tools/baker.
 *
 * Le formule sono identiche a quelle di prima, cifra per cifra. Erano tarate
 * guardando il mondo, e cambiarle qui significherebbe cambiare il mondo.
 * ========================================================================== */
#ifndef WORLDGEN_H
#define WORLDGEN_H

#include "worldtypes.h"
#include "config.h"

/* Tutto cio' che serve a generare: il seme e i villaggi, che spianano il
 * terreno e quindi entrano nel calcolo delle quote. */
typedef struct {
    unsigned int seed;
    Town         towns[MAX_TOWNS];
    int          townCount;
    Vector3      cryptPos;

    /* Heightmap opzionale che prende il posto del rumore: un PNG in scala di
     * grigi prodotto con QGIS/GDAL, Blender A.N.T. Landscape o a mano. Era una
     * possibilita' del gioco (docs/03); ora che il terreno si cuoce, il posto
     * giusto per leggerla e' qui. */
    Image        heightmap;
    bool         useHeightmap;
} WorldGen;

/* Semina i villaggi e la cripta. Da chiamare prima di ogni altra cosa: le quote
 * dipendono dai villaggi. 'heightmap' puo' essere NULL: allora il terreno viene
 * dal rumore. */
void    GenInit(WorldGen *g, unsigned int seed, const Image *heightmap);

float   GenHeight(const WorldGen *g, float x, float z);
Vector3 GenNormal(const WorldGen *g, float x, float z);
Biome   GenBiome(const WorldGen *g, float x, float z);
/* Come GenBiome ma con la quota gia' nota: il baker la ha appena calcolata, e
 * ricalcolarla raddoppierebbe il costo del bake. */
Biome   GenBiomeAtHeight(const WorldGen *g, float x, float z, float h);
Vector3 GenSafeSpawn(const WorldGen *g, float x, float z);

/* Riempie 'out' con i prop del chunk (cx,cz) e ritorna quanti sono.
 * 'cap' e' la capienza di 'out'; oltre quella i prop in eccesso si perdono,
 * esattamente come faceva AddProp() in gioco. */
int     GenChunkProps(const WorldGen *g, int cx, int cz, Prop *out, int cap);

/* Organico previsto per un villaggio: gli stessi tipi e le stesse posizioni che
 * SpawnTownNPCs() metteva nel codice. Ritorna quanti ne ha scritti. */
int     GenTownNpcs(const WorldGen *g, int townIndex, NpcSpawn *out, int cap);

/* Punto di partenza del giocatore. */
Vector3 GenPlayerStart(const WorldGen *g);

#endif /* WORLDGEN_H */
