/* ============================================================================
 * worldfmt.h - Formato del mondo cotto (assets/world/).
 *
 * Il seme e' un innesco, non un ingresso: tools/baker lo usa una volta per
 * scrivere questi file, e da quel momento la sorgente di verita' e' il mondo
 * cotto - modificabile a mano o da un editor (fase 3 del piano in docs/05).
 *
 *   manifest.txt   seme d'origine, dimensioni, risoluzione, versione (testo)
 *   height.bin     WORLD_GRID x WORLD_GRID quote in uint16, 1 cm di precisione
 *   biome.bin      WORLD_GRID x WORLD_GRID biomi in uint8
 *   props.bin      prop indicizzati per chunk, 10 byte per istanza
 *   spawns.txt     villaggi, organico degli NPC, cripta, inizio (testo)
 *   grain.png      grana del terreno, cotta perche' nasceva dal rumore
 *
 * Binario per cio' che e' voluminoso, testo per cio' che una persona vuole
 * aprire e correggere.
 *
 * PERCHE' UNA DIRECTORY DI CHUNK: props.bin e' progettato per la SCRITTURA.
 * Ogni chunk ha una voce con posizione, conteggio e capienza: spostare un
 * albero significa riscrivere 10 byte, aggiungerne uno oltre la capienza
 * significa accodare in fondo e aggiornare 8 byte di indice. Nessuna operazione
 * dell'editor deve riscrivere 14 MB.
 *
 * QUANTIZZAZIONE: le funzioni di codifica stanno qui, non nel baker, perche'
 * chi scrive e chi legge devono usare esattamente le stesse - altrimenti il
 * mondo caricato non e' quello generato.
 *
 * ORDINE DEI BYTE: i file si scrivono nell'ordine nativo della macchina. Il
 * numero magico lo rivela: se arriva rovesciato il caricatore rifiuta il file
 * invece di leggere quote assurde.
 * ========================================================================== */
#ifndef WORLDFMT_H
#define WORLDFMT_H

#include "config.h"
#include <stdint.h>

#define WORLD_FMT_VERSION 1

/* --- Griglia delle quote ------------------------------------------------
 * Risoluzione 2 m, la stessa della mesh: i vertici del terreno cadono
 * esattamente sui campioni, quindi il terreno caricato coincide con quello
 * generato invece di essere una sua interpolazione. A 1 m il file passerebbe da
 * 8,4 a 33 MB e servirebbe infittire anche la mesh. */
#define WORLD_GRID       (WORLD_CHUNKS * CHUNK_QUADS + 1)   /* 2049 campioni  */
#define WORLD_GRID_STEP  VERT_STEP                          /* 2 m            */

/* Quote in uint16: 1 cm di precisione, da -100 m a +555 m. Il gioco arriva a
 * 165 m sul livello del mare, quindi il margine e' abbondante. */
#define WORLD_H_BIAS   100.0f
#define WORLD_H_SCALE  100.0f

/* --- Intestazioni dei file binari --------------------------------------- */
#define WORLD_MAGIC_HEIGHT 0x54484D46u   /* "FMHT" */
#define WORLD_MAGIC_BIOME  0x49424D46u   /* "FMBI" */
#define WORLD_MAGIC_PROPS  0x52504D46u   /* "FMPR" */

/* Comune a height.bin e biome.bin: 16 byte, poi i campioni per righe di z. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t width;      /* campioni lungo x */
    uint32_t height;     /* campioni lungo z */
} WorldGridHeader;

/* props.bin: intestazione, directory di WORLD_CHUNKS^2 voci, poi i record. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t chunkCount;   /* WORLD_CHUNKS * WORLD_CHUNKS */
    uint32_t recordSize;   /* sizeof(WorldPropRec), per rifiutare formati altrui */
    uint32_t recordCount;  /* record scritti nel file, riserve comprese */
    uint32_t dataOffset;   /* byte dall'inizio del file al primo record */
} WorldPropsHeader;

typedef struct {
    uint32_t first;    /* indice del primo record del chunk */
    uint16_t count;    /* quanti sono usati */
    uint16_t capacity; /* quanti ce ne stanno prima del chunk successivo */
} WorldChunkEntry;

/* 10 byte per istanza. Posizione locale al chunk in uint16 (0,1 cm), quota
 * nella stessa codifica della griglia, rotazione a 1,4 gradi - invisibile su un
 * albero, e su una casa e' meno di quanto si noti. 'taken' non c'e': raccogliere
 * un'erba e' stato della partita, non del mondo, e vive nel salvataggio. */
typedef struct {
    uint16_t lx;      /* x locale: 0..CHUNK_SIZE */
    uint16_t lz;
    uint16_t y;       /* quota assoluta, stessa codifica di height.bin */
    uint8_t  type;    /* PropType */
    uint8_t  scale;   /* 1/64 di unita': 0..3,98 */
    uint8_t  rot;     /* 360/256 gradi */
    uint8_t  radius;  /* 5 cm: 0..12,75 m. 0 = attraversabile */
} WorldPropRec;

/* --- Codifica ----------------------------------------------------------- */

static inline uint16_t WorldEncodeHeight(float h)
{
    float v = (h + WORLD_H_BIAS) * WORLD_H_SCALE + 0.5f;
    if (v < 0.0f)       v = 0.0f;
    if (v > 65535.0f)   v = 65535.0f;
    return (uint16_t)v;
}

static inline float WorldDecodeHeight(uint16_t v)
{
    return (float)v / WORLD_H_SCALE - WORLD_H_BIAS;
}

static inline uint16_t WorldEncodeLocal(float v)
{
    float t = v / CHUNK_SIZE;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (uint16_t)(t * 65535.0f + 0.5f);
}

static inline float WorldDecodeLocal(uint16_t v)
{
    return (float)v * (CHUNK_SIZE / 65535.0f);
}

static inline uint8_t WorldEncodeScale(float s)
{
    float v = s * 64.0f + 0.5f;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (uint8_t)v;
}

static inline float WorldDecodeScale(uint8_t v) { return (float)v / 64.0f; }

static inline uint8_t WorldEncodeRot(float deg)
{
    float d = deg;
    while (d <    0.0f) d += 360.0f;
    while (d >= 360.0f) d -= 360.0f;
    return (uint8_t)(d * (256.0f / 360.0f) + 0.5f);
}

static inline float WorldDecodeRot(uint8_t v) { return (float)v * (360.0f / 256.0f); }

static inline uint8_t WorldEncodeRadius(float r)
{
    float v = r * 20.0f + 0.5f;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (uint8_t)v;
}

static inline float WorldDecodeRadius(uint8_t v) { return (float)v / 20.0f; }

/* --- Nomi dei file ------------------------------------------------------ */
#define WORLD_DIR        "assets/world"
#define WORLD_MANIFEST   "manifest.txt"
#define WORLD_HEIGHT     "height.bin"
#define WORLD_BIOME      "biome.bin"
#define WORLD_PROPS      "props.bin"
#define WORLD_SPAWNS     "spawns.txt"
#define WORLD_GRAIN      "grain.png"

/* Slack lasciato in coda a ogni chunk quando il baker scrive props.bin: le
 * prime aggiunte dell'editor entrano senza accodare in fondo al file. */
#define WORLD_PROP_SLACK 8

#endif /* WORLDFMT_H */
