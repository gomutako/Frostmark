#include "worldio.h"
#include "dataparse.h"
#include "fmath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------ */
/*  LETTURA DEI FILE BINARI                                                 */
/* ------------------------------------------------------------------------ */

static void Join(char *dest, int size, const char *dir, const char *name)
{
    snprintf(dest, (size_t)size, "%s/%s", dir, name);
}

/* Legge una griglia (height.bin o biome.bin) controllando intestazione e
 * dimensione del file: un file troncato o di un altro mondo va rifiutato, non
 * letto a metà. Ritorna il buffer posseduto, NULL se qualcosa non torna. */
static void *LoadGrid(const char *path, uint32_t magic, int sampleSize,
                      int expectW, int expectH)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        DataProblem(NULL, "%s: non si apre. Cuoci il mondo con 'make mondo'.", path);
        return NULL;
    }

    WorldGridHeader hd;
    if (fread(&hd, sizeof(hd), 1, f) != 1) {
        DataProblem(NULL, "%s: intestazione illeggibile (file troncato?)", path);
        fclose(f);
        return NULL;
    }
    if (hd.magic != magic) {
        /* Se il numero magico arriva rovesciato il file viene da una macchina
         * con l'altro ordine dei byte: meglio dirlo che leggere quote assurde. */
        DataProblem(NULL, "%s: non e' il file che dichiara di essere "
                          "(magic %08x, atteso %08x). Ricuocilo su questa macchina.",
                    path, hd.magic, magic);
        fclose(f);
        return NULL;
    }
    if (hd.version != WORLD_FMT_VERSION) {
        DataProblem(NULL, "%s: formato versione %u, questo gioco legge la %d.",
                    path, hd.version, WORLD_FMT_VERSION);
        fclose(f);
        return NULL;
    }
    if ((int)hd.width != expectW || (int)hd.height != expectH) {
        DataProblem(NULL, "%s: griglia %ux%u, attesa %dx%d.",
                    path, hd.width, hd.height, expectW, expectH);
        fclose(f);
        return NULL;
    }

    size_t count = (size_t)expectW * (size_t)expectH;
    void  *buf   = malloc(count * (size_t)sampleSize);
    if (buf == NULL) {
        DataProblem(NULL, "%s: memoria insufficiente (%zu byte).",
                    path, count * (size_t)sampleSize);
        fclose(f);
        return NULL;
    }
    if (fread(buf, (size_t)sampleSize, count, f) != count) {
        DataProblem(NULL, "%s: file troncato, mancano dei campioni.", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}

static bool LoadProps(WorldIo *io, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        DataProblem(NULL, "%s: non si apre. Cuoci il mondo con 'make mondo'.", path);
        return false;
    }

    WorldPropsHeader hd;
    if (fread(&hd, sizeof(hd), 1, f) != 1) {
        DataProblem(NULL, "%s: intestazione illeggibile.", path);
        fclose(f);
        return false;
    }

    const uint32_t chunks = (uint32_t)(WORLD_CHUNKS * WORLD_CHUNKS);
    if (hd.magic != WORLD_MAGIC_PROPS) {
        DataProblem(NULL, "%s: numero magico %08x, atteso %08x.",
                    path, hd.magic, WORLD_MAGIC_PROPS);
        fclose(f);
        return false;
    }
    if (hd.version != WORLD_FMT_VERSION) {
        DataProblem(NULL, "%s: formato versione %u, questo gioco legge la %d.",
                    path, hd.version, WORLD_FMT_VERSION);
        fclose(f);
        return false;
    }
    if (hd.chunkCount != chunks) {
        DataProblem(NULL, "%s: %u chunk, attesi %u.", path, hd.chunkCount, chunks);
        fclose(f);
        return false;
    }
    if (hd.recordSize != (uint32_t)sizeof(WorldPropRec)) {
        DataProblem(NULL, "%s: record di %u byte, questo gioco ne legge %zu.",
                    path, hd.recordSize, sizeof(WorldPropRec));
        fclose(f);
        return false;
    }

    io->chunkDir = (WorldChunkEntry *)malloc(sizeof(WorldChunkEntry) * chunks);
    io->props    = (WorldPropRec *)malloc(sizeof(WorldPropRec) * (hd.recordCount ? hd.recordCount : 1));
    if (io->chunkDir == NULL || io->props == NULL) {
        DataProblem(NULL, "%s: memoria insufficiente.", path);
        fclose(f);
        return false;
    }

    if (fread(io->chunkDir, sizeof(WorldChunkEntry), chunks, f) != chunks) {
        DataProblem(NULL, "%s: directory dei chunk troncata.", path);
        fclose(f);
        return false;
    }
    if (fseek(f, (long)hd.dataOffset, SEEK_SET) != 0) {
        DataProblem(NULL, "%s: dataOffset %u fuori dal file.", path, hd.dataOffset);
        fclose(f);
        return false;
    }
    if (hd.recordCount > 0 &&
        fread(io->props, sizeof(WorldPropRec), hd.recordCount, f) != hd.recordCount) {
        DataProblem(NULL, "%s: file troncato, mancano dei prop.", path);
        fclose(f);
        return false;
    }
    fclose(f);

    io->propCount = (int)hd.recordCount;

    /* La directory arriva da un file modificabile: una voce che punta oltre i
     * record letti farebbe leggere memoria altrui. Si controlla adesso, una
     * volta, invece che a ogni chunk caricato. */
    io->propLive = 0;
    for (uint32_t i = 0; i < chunks; i++) {
        const WorldChunkEntry *e = &io->chunkDir[i];
        io->propLive += e->count;
        if ((size_t)e->first + (size_t)e->count > (size_t)io->propCount) {
            DataProblem(NULL, "%s: il chunk %u,%u dichiara %u prop dall'indice %u "
                              "ma il file ne contiene %d.",
                        path, i % WORLD_CHUNKS, i / WORLD_CHUNKS,
                        e->count, e->first, io->propCount);
            return false;
        }
        if (e->count > e->capacity) {
            DataProblem(NULL, "%s: il chunk %u,%u ha %u prop in %u posti.",
                        path, i % WORLD_CHUNKS, i / WORLD_CHUNKS,
                        e->count, e->capacity);
            return false;
        }
    }

    /* Un tipo fuori intervallo pescherebbe un modello oltre la tabella. */
    for (int i = 0; i < io->propCount; i++) {
        if (io->props[i].type >= PROP_COUNT) {
            DataProblem(NULL, "%s: prop %d di tipo %u, ammessi 0..%d.",
                        path, i, io->props[i].type, PROP_COUNT - 1);
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------ */
/*  MANIFEST E PUNTI DI SPAWN                                               */
/* ------------------------------------------------------------------------ */

/* "1234.5 28.3 2048.0" -> Vector3. La quota puo' mancare: la si ricava dal
 * terreno, ed e' il caso normale quando una posizione e' scritta a mano. */
static bool ParseVec(const DataReader *r, const char *key, const char *value,
                     float *x, float *y, float *z, bool *hasY)
{
    float a = 0.0f, b = 0.0f, c = 0.0f;
    int n = sscanf(value, "%f %f %f", &a, &b, &c);
    if (n == 3) { *x = a; *y = b; *z = c; if (hasY) *hasY = true;  return true; }
    if (n == 2) { *x = a; *y = 0.0f; *z = b; if (hasY) *hasY = false; return true; }
    DataProblem(r, "'%s': attese due (x z) o tre (x y z) coordinate, letto '%s'",
                key, value);
    return false;
}

static bool LoadManifest(WorldIo *io, const char *path)
{
    DataReader r;
    if (!DataOpen(&r, path)) return false;

    bool haveSeed = false, haveGrid = false;
    int  version = 0, chunks = 0, samples = 0;
    float side = 0.0f, step = 0.0f;
    io->minHeight = 0.0f;
    io->maxHeight = 0.0f;

    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "mondo") != 0) {
            DataProblem(&r, "sezione '%s' sconosciuta: il manifest ha solo [mondo]",
                        r.kind);
            DataSkipSection(&r);
            continue;
        }
        char *k, *v;
        while (DataNextField(&r, &k, &v)) {
            if      (strcmp(k, "versione") == 0)  DataAsInt(&r, k, v, 1, 999, &version);
            else if (strcmp(k, "seme") == 0) {
                io->seed = (unsigned int)strtoul(v, NULL, 10);
                haveSeed = true;
            }
            else if (strcmp(k, "lato_metri") == 0)    DataAsFloat(&r, k, v, 1.0f, 1e7f, &side);
            else if (strcmp(k, "chunk") == 0)         DataAsInt(&r, k, v, 1, 4096, &chunks);
            else if (strcmp(k, "passo_griglia") == 0) DataAsFloat(&r, k, v, 0.05f, 64.0f, &step);
            else if (strcmp(k, "campioni") == 0)    { DataAsInt(&r, k, v, 2, 65535, &samples); haveGrid = true; }
            else if (strcmp(k, "quota_minima") == 0)  DataAsFloat(&r, k, v, -1000.0f, 1000.0f, &io->minHeight);
            else if (strcmp(k, "quota_massima") == 0) DataAsFloat(&r, k, v, -1000.0f, 1000.0f, &io->maxHeight);
            else if (strcmp(k, "prop") == 0 || strcmp(k, "generato") == 0) {
                /* informativi: li scrive il baker perche' li legga una persona */
            }
            else DataProblem(&r, "chiave '%s' sconosciuta nel manifest", k);
        }
    }
    DataClose(&r);

    if (!haveSeed) DataProblem(NULL, "%s: manca 'seme'.", path);
    if (!haveGrid) DataProblem(NULL, "%s: manca 'campioni'.", path);

    /* Il mondo cotto e la build devono essere d'accordo sulla geometria: se non
     * lo sono, ogni collisione e ogni mesh sarebbero fuori posto di metri. */
    if (version != WORLD_FMT_VERSION)
        DataProblem(NULL, "%s: versione %d, questo gioco legge la %d.",
                    path, version, WORLD_FMT_VERSION);
    if (chunks != WORLD_CHUNKS)
        DataProblem(NULL, "%s: %d chunk, questa build ne vuole %d (config.h).",
                    path, chunks, WORLD_CHUNKS);
    if (fabsf(side - WORLD_SIZE) > 0.01f)
        DataProblem(NULL, "%s: lato %.1f m, questa build vuole %.1f m (config.h).",
                    path, side, (double)WORLD_SIZE);
    if (fabsf(step - WORLD_GRID_STEP) > 0.001f)
        DataProblem(NULL, "%s: passo %.3f m, questa build vuole %.3f m.",
                    path, step, (double)WORLD_GRID_STEP);
    if (samples != WORLD_GRID)
        DataProblem(NULL, "%s: %d campioni per lato, attesi %d.",
                    path, samples, WORLD_GRID);

    io->gridW    = WORLD_GRID;
    io->gridH    = WORLD_GRID;
    io->gridStep = WORLD_GRID_STEP;
    return DataProblemCount() == 0;
}

static bool LoadSpawns(WorldIo *io, const char *path)
{
    DataReader r;
    if (!DataOpen(&r, path)) return false;

    io->townCount = 0;
    io->npcCount  = 0;
    bool haveStart = false, haveCrypt = false;

    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "villaggio") == 0) {
            if (io->townCount >= MAX_TOWNS) {
                DataProblem(&r, "piu' di %d villaggi: alza MAX_TOWNS in config.h",
                            MAX_TOWNS);
                DataSkipSection(&r);
                continue;
            }
            int   index = io->townCount;
            Town *t     = &io->towns[index];
            memset(t, 0, sizeof(*t));
            t->radius = TOWN_RADIUS;

            bool havePos = false, townBase = false;
            char *k, *v;
            while (DataNextField(&r, &k, &v)) {
                if (strcmp(k, "nome") == 0)
                    DataAsText(&r, k, v, t->name, (int)sizeof(t->name));
                else if (strcmp(k, "posizione") == 0) {
                    bool hasY = false;
                    if (ParseVec(&r, k, v, &t->pos.x, &t->pos.y, &t->pos.z, &hasY)) {
                        havePos = true;
                        /* Chi sposta un villaggio a mano scrive due coordinate:
                         * la quota la sa il terreno, ed e' gia' caricato. */
                        if (!hasY) t->pos.y = WorldIoHeight(io, t->pos.x, t->pos.z);
                    }
                }
                else if (strcmp(k, "raggio") == 0)
                    DataAsFloat(&r, k, v, 5.0f, 500.0f, &t->radius);
                else if (strcmp(k, "quota_base") == 0)
                    townBase = DataAsFloat(&r, k, v, -100.0f, 500.0f, &t->baseHeight);
                else if (strcmp(k, "npc") == 0) {
                    /* npc = <tipo> <x> <z> */
                    char type[24] = { 0 };
                    float nx = 0.0f, nz = 0.0f;
                    if (sscanf(v, "%23s %f %f", type, &nx, &nz) != 3) {
                        DataProblem(&r, "'npc': atteso '<tipo> <x> <z>', letto '%s'", v);
                    } else if (io->npcCount >= MAX_WORLD_NPCS) {
                        DataProblem(&r, "piu' di %d NPC: alza MAX_WORLD_NPCS in config.h",
                                    MAX_WORLD_NPCS);
                    } else {
                        NpcSpawn *s = &io->npcs[io->npcCount++];
                        TextCopy(s->type, type);
                        s->x = nx; s->z = nz;
                        s->townIndex = index;
                    }
                }
                else DataProblem(&r, "chiave '%s' sconosciuta in [villaggio]", k);
            }

            if (t->name[0] == '\0')
                DataProblemAt(&r, r.kindLine, "villaggio senza 'nome'");
            if (!havePos)
                DataProblemAt(&r, r.kindLine, "villaggio '%s' senza 'posizione'", t->name);
            /* La quota base spiana il terreno del villaggio: se non e' scritta,
             * e' quella del centro. */
            if (!townBase) t->baseHeight = t->pos.y;
            io->townCount++;
        }
        else if (strcmp(r.kind, "cripta") == 0) {
            char *k, *v;
            while (DataNextField(&r, &k, &v)) {
                if (strcmp(k, "posizione") == 0) {
                    bool hasY = false;
                    if (ParseVec(&r, k, v, &io->cryptPos.x, &io->cryptPos.y,
                                 &io->cryptPos.z, &hasY)) {
                        haveCrypt = true;
                        if (!hasY)
                            io->cryptPos.y = WorldIoHeight(io, io->cryptPos.x,
                                                           io->cryptPos.z);
                    }
                }
                else DataProblem(&r, "chiave '%s' sconosciuta in [cripta]", k);
            }
        }
        else if (strcmp(r.kind, "inizio") == 0) {
            char *k, *v;
            while (DataNextField(&r, &k, &v)) {
                if (strcmp(k, "posizione") == 0) {
                    bool hasY = false;
                    if (ParseVec(&r, k, v, &io->playerStart.x, &io->playerStart.y,
                                 &io->playerStart.z, &hasY)) {
                        haveStart = true;
                        if (!hasY)
                            io->playerStart.y = WorldIoHeight(io, io->playerStart.x,
                                                              io->playerStart.z);
                    }
                }
                else DataProblem(&r, "chiave '%s' sconosciuta in [inizio]", k);
            }
        }
        else {
            DataProblem(&r, "sezione '%s' sconosciuta: spawns.txt ha "
                            "[villaggio], [cripta] e [inizio]", r.kind);
            DataSkipSection(&r);
        }
    }
    DataClose(&r);

    if (io->townCount == 0)
        DataProblem(NULL, "%s: nessun villaggio. Il gioco parte in un villaggio.", path);
    if (!haveCrypt)
        DataProblem(NULL, "%s: manca [cripta]: e' l'obiettivo della quest principale.", path);
    if (!haveStart)
        DataProblem(NULL, "%s: manca [inizio]: non si saprebbe dove mettere il giocatore.", path);

    return DataProblemCount() == 0;
}

/* ------------------------------------------------------------------------ */
/*  CARICAMENTO                                                             */
/* ------------------------------------------------------------------------ */

bool WorldIoLoad(WorldIo *io, const char *dir)
{
    memset(io, 0, sizeof(*io));
    TextCopy(io->path, dir);

    char path[256];

    Join(path, sizeof(path), dir, WORLD_MANIFEST);
    if (!LoadManifest(io, path)) return false;

    Join(path, sizeof(path), dir, WORLD_HEIGHT);
    io->height = (uint16_t *)LoadGrid(path, WORLD_MAGIC_HEIGHT, (int)sizeof(uint16_t),
                                      io->gridW, io->gridH);
    if (io->height == NULL) return false;

    Join(path, sizeof(path), dir, WORLD_BIOME);
    io->biome = (uint8_t *)LoadGrid(path, WORLD_MAGIC_BIOME, (int)sizeof(uint8_t),
                                    io->gridW, io->gridH);
    if (io->biome == NULL) return false;

    /* Un bioma fuori intervallo diventerebbe un colore letto oltre la tabella. */
    size_t samples = (size_t)io->gridW * (size_t)io->gridH;
    for (size_t i = 0; i < samples; i++) {
        if (io->biome[i] >= BIOME_COUNT) {
            DataProblem(NULL, "%s: bioma %u al campione %zu, ammessi 0..%d.",
                        path, io->biome[i], i, BIOME_COUNT - 1);
            return false;
        }
    }

    Join(path, sizeof(path), dir, WORLD_PROPS);
    if (!LoadProps(io, path)) return false;

    Join(path, sizeof(path), dir, WORLD_SPAWNS);
    if (!LoadSpawns(io, path)) return false;

    TraceLog(LOG_INFO, "WORLD: mondo cotto da %s (seme %u, %dx%d campioni, "
                       "%d prop, %d villaggi, %d NPC)",
             dir, io->seed, io->gridW, io->gridH, io->propLive,
             io->townCount, io->npcCount);
    return true;
}

void WorldIoFree(WorldIo *io)
{
    free(io->height);
    free(io->biome);
    free(io->chunkDir);
    free(io->props);
    io->height   = NULL;
    io->biome    = NULL;
    io->chunkDir = NULL;
    io->props    = NULL;
    io->propCount = 0;
}

/* ------------------------------------------------------------------------ */
/*  INTERROGAZIONE                                                          */
/* ------------------------------------------------------------------------ */

float WorldIoHeight(const WorldIo *io, float x, float z)
{
    if (io->height == NULL) return 0.0f;

    float u = FmClamp(x / io->gridStep, 0.0f, (float)(io->gridW - 1));
    float v = FmClamp(z / io->gridStep, 0.0f, (float)(io->gridH - 1));

    int x0 = (int)u, z0 = (int)v;
    int x1 = (x0 + 1 < io->gridW) ? x0 + 1 : x0;
    int z1 = (z0 + 1 < io->gridH) ? z0 + 1 : z0;
    float fx = u - (float)x0, fz = v - (float)z0;

    const uint16_t *g = io->height;
    float h00 = WorldDecodeHeight(g[(size_t)z0 * io->gridW + x0]);
    float h10 = WorldDecodeHeight(g[(size_t)z0 * io->gridW + x1]);
    float h01 = WorldDecodeHeight(g[(size_t)z1 * io->gridW + x0]);
    float h11 = WorldDecodeHeight(g[(size_t)z1 * io->gridW + x1]);

    return FmLerp(FmLerp(h00, h10, fx), FmLerp(h01, h11, fx), fz);
}

Biome WorldIoBiome(const WorldIo *io, float x, float z)
{
    if (io->biome == NULL) return BIOME_OCEAN;

    float u = FmClamp(x / io->gridStep, 0.0f, (float)(io->gridW - 1));
    float v = FmClamp(z / io->gridStep, 0.0f, (float)(io->gridH - 1));
    int xi = (int)(u + 0.5f), zi = (int)(v + 0.5f);
    if (xi >= io->gridW) xi = io->gridW - 1;
    if (zi >= io->gridH) zi = io->gridH - 1;

    return (Biome)io->biome[(size_t)zi * io->gridW + xi];
}

int WorldIoChunkProps(const WorldIo *io, int cx, int cz, Prop *out, int cap)
{
    if (io->chunkDir == NULL) return 0;
    if (cx < 0 || cz < 0 || cx >= WORLD_CHUNKS || cz >= WORLD_CHUNKS) return 0;

    const WorldChunkEntry *e = &io->chunkDir[(size_t)cz * WORLD_CHUNKS + cx];
    float ox = (float)cx * CHUNK_SIZE;
    float oz = (float)cz * CHUNK_SIZE;

    int n = 0;
    for (int i = 0; i < (int)e->count && n < cap; i++) {
        const WorldPropRec *rec = &io->props[e->first + (uint32_t)i];
        Prop *p = &out[n++];
        p->pos.x  = ox + WorldDecodeLocal(rec->lx);
        p->pos.z  = oz + WorldDecodeLocal(rec->lz);
        p->pos.y  = WorldDecodeHeight(rec->y);
        p->type   = (PropType)rec->type;
        p->scale  = WorldDecodeScale(rec->scale);
        p->rot    = WorldDecodeRot(rec->rot);
        p->radius = WorldDecodeRadius(rec->radius);
        p->taken  = false;
    }
    return n;
}
