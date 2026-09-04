/* ============================================================================
 * world.h - Mondo aperto: altimetria, biomi, streaming dei chunk, oggetti.
 *
 * IDEA CHIAVE: il mondo e' un DATO, non una funzione. Viene cotto una volta da
 * un seme (tools/baker) e da quel momento assets/world/ e' la sorgente di
 * verita': si carica, si interroga e si potra' modificare. Prima della fase 3
 * del piano (docs/05) l'altezza era una funzione pura di (seme, x, z) e il mondo
 * si rigenerava a ogni avvio; adesso spostare un albero o spianare una radura e'
 * una modifica che resta, ed e' il presupposto della fisica.
 *
 * Cio' che resta qui: mesh dei chunk, streaming, disegno, collisioni. La
 * generazione e' in tools/worldgen.c, il caricamento in worldio.c.
 * ========================================================================== */
#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "config.h"
#include "instancing.h"
#include "worldtypes.h"
#include "worldio.h"
#include <stdbool.h>

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
    /* Il mondo cotto: quote, biomi, prop, villaggi. Tutto cio' che segue e'
     * derivato da qui o serve a disegnarlo. */
    WorldIo io;

    unsigned int seed;             /* seme d'origine, dal manifest: identita' */

    /* Copie da 'io', perche' mezzo gioco le legge come w->towns[i]. */
    Town  towns[MAX_TOWNS];
    int   townCount;
    Vector3 cryptPos;              /* obiettivo della quest principale */

    Chunk chunks[MAX_LOADED_CHUNKS];

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
    /* Un gruppo di lotti per tipo, uno per mesh del modello. Vuoto vuol dire
     * "disegna un oggetto per volta, come prima". */
    InstModel propBatch[PROP_COUNT];

    /* Pezzi modulari degli edifici: vedi BUILD_FILES in world.c. Casa e torre
     * non esistono come modello unico nei kit CC0, si compongono. */
    Model  buildPart[BUILD_PART_COUNT];
    bool   hasBuildParts;
    /* Un gruppo per tipo di pezzo: una casa bassa costa 19 chiamate, una alta
     * 45, e i tipi di pezzo sono dieci. */
    InstModel partBatch[BUILD_PART_COUNT];
} World;

/* Carica il mondo cotto da 'dir' e prepara le risorse grafiche. false se il
 * mondo manca o e' incoerente: non esiste un mondo di ripiego, e i problemi si
 * contano con DataProblemCount(). Richiede una finestra aperta (crea texture).
 * Vedi WorldValidate() per il controllo che si fa prima di aprirla. */
bool    WorldInit(World *w, const char *dir);
void    WorldUnload(World *w);

/* Carica e butta via: dice se il mondo cotto e' leggibile e coerente senza
 * bisogno di un contesto grafico. La usano ./frostmark --valida e l'avvio, che
 * preferisce non aprire una finestra su un mondo rotto. */
bool    WorldValidate(const char *dir);

float   WorldHeight(const World *w, float x, float z);
Vector3 WorldNormalAt(const World *w, float x, float z);
Biome   WorldBiomeAt(const World *w, float x, float z);
Color   WorldBiomeColor(Biome b);
const char *WorldBiomeName(Biome b);

void    WorldUpdateStreaming(World *w, Vector3 center);
void    WorldDrawTerrain(World *w, Camera3D cam, Color tint);
void    WorldDrawProps(World *w, Camera3D cam, Color tint);
/* Terreno e prop entro 'radius' dal centro, senza scarto per cono visivo:
 * serve al passaggio d'ombra. */
void    WorldDrawShadowCasters(World *w, Vector3 center, float radius);
void    WorldDrawWater(const World *w, Vector3 camPos, Color tint, float t);

/* Spinge fuori 'pos' dai prop solidi vicini (collisione a cerchi). */
void    WorldResolveCollision(World *w, Vector3 *pos, float radius);
/* true se il punto e' dentro le mura di una casa: la camera in terza persona
 * si accorcia, altrimenti resterebbe fuori a inquadrare un muro. */
bool    WorldInsideBuilding(const World *w, Vector3 pos);
/* La superficie calpestabile piu' alta sotto 'pos', entro 'reach' sopra i
 * piedi: solai e scale degli edifici alti. -1e9 se non ce n'e' nessuna. */
float   WorldSupportHeight(const World *w, Vector3 pos, float reach);
/* Quanti piani ha una casa: 1 o 2, deciso dalla sua posizione. */
int     WorldHouseFloors(const Prop *p);
/* Quanto puo' arretrare la camera da 'eye' lungo 'dir' senza che un edificio
 * finisca fra lei e il giocatore. */
float   WorldCameraClip(const World *w, Vector3 eye, Vector3 dir, float maxDist);
/* Ritorna il prop raccoglibile piu' vicino (o NULL). */
Prop   *WorldNearestProp(World *w, Vector3 pos, float maxDist, PropType type);
/* Punto sicuro (terra emersa) piu' vicino a (x,z). */
Vector3 WorldSafeSpawn(const World *w, float x, float z);

#endif /* WORLD_H */
