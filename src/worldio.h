/* ============================================================================
 * worldio.h - Caricamento del mondo cotto (assets/world/).
 *
 * Prende il posto della generazione procedurale che stava in world.c: il
 * terreno non e' piu' una funzione del seme, e' un dato da caricare. Il formato
 * e' descritto in worldfmt.h, lo scrive tools/baker.
 *
 * Quote e biomi si caricano interi in memoria (12,6 MB): a 2 m di risoluzione
 * il mondo intero costa meno di una texture 2K, e streammarli complicherebbe
 * ogni chiamante per niente. I prop (1,7 MB) restano indicizzati per chunk,
 * cosi' lo streaming a 121 chunk continua a funzionare come prima.
 *
 * Nessun valore di ripiego: se assets/world/ manca o e' incoerente il gioco non
 * parte e dice quale file e cosa. Il mondo si ricuoce con 'make mondo'.
 *
 * Vedi docs/05-piano-dati-esterni-e-motore.md, fase 3.
 * ========================================================================== */
#ifndef WORLDIO_H
#define WORLDIO_H

#include "worldtypes.h"
#include "worldfmt.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char         path[192];      /* cartella di provenienza */
    unsigned int seed;           /* seme d'origine: identita' del mondo */
    int          gridW, gridH;
    float        gridStep;

    uint16_t    *height;         /* gridW * gridH, codifica di worldfmt.h */
    uint8_t     *biome;

    WorldChunkEntry *chunkDir;   /* WORLD_CHUNKS * WORLD_CHUNKS voci */
    WorldPropRec    *props;
    int              propCount;  /* record letti, posti di riserva compresi */
    int              propLive;   /* prop veri, somma dei conteggi dei chunk  */

    Town     towns[MAX_TOWNS];
    int      townCount;
    Vector3  cryptPos;
    Vector3  playerStart;
    NpcSpawn npcs[MAX_WORLD_NPCS];
    int      npcCount;

    /* Statistiche del manifest, mostrate all'avvio e utili a capire subito se
     * si sta giocando un mondo diverso da quello che si crede. */
    float    minHeight, maxHeight;
} WorldIo;

/* Carica tutto da 'dir'. I problemi si contano con DataProblemCount(). */
bool  WorldIoLoad(WorldIo *io, const char *dir);
void  WorldIoFree(WorldIo *io);

/* Quota interpolata bilinearmente sulla griglia. Sui vertici della mesh, che
 * cadono esattamente sui campioni, restituisce il valore cotto senza
 * interpolare: il terreno disegnato e' quello generato. */
float WorldIoHeight(const WorldIo *io, float x, float z);
/* Bioma del campione piu' vicino. */
Biome WorldIoBiome(const WorldIo *io, float x, float z);

/* Prop del chunk (cx,cz) decodificati in coordinate del mondo.
 * Ritorna quanti ne ha scritti, al massimo 'cap'. */
int   WorldIoChunkProps(const WorldIo *io, int cx, int cz, Prop *out, int cap);

#endif /* WORLDIO_H */
