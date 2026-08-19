/* ============================================================================
 * baker.c - Cuoce il mondo dal seme in assets/world/.
 *
 *   ./baker                      cuoce con il seme predefinito in assets/world/
 *   ./baker --seme 12345         cuoce un altro mondo
 *   ./baker --out /tmp/mondo     scrive altrove
 *   ./baker --heightmap m.png    prende le quote da un PNG invece dal rumore
 *   ./baker --verifica           ricarica cio' che c'e' e lo confronta con il
 *                                mondo generato dal seme del manifest
 *
 * Si esegue una volta a inizio progetto, non a ogni avvio: dopo il bake il
 * mondo cotto e' la sorgente di verita' e si modifica, non si rigenera.
 * Rigenerarlo cancella le modifiche fatte a mano - per questo il baker rifiuta
 * di sovrascrivere senza --forza.
 *
 * Vedi docs/05-piano-dati-esterni-e-motore.md, fase 3.
 * ========================================================================== */
#include "worldgen.h"
#include "worldfmt.h"
#include "worldio.h"
#include "noise.h"
#include "fmath.h"
#include "dataparse.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define BAKER_SEED_DEFAULT 20260819u

/* Tutti i prop del mondo, costruiti in memoria prima di scrivere: 4096 chunk
 * per 160 posti sono 655.360 record al massimo, 6,5 MB. Comodo e transitorio. */
#define BAKER_MAX_PROPS (WORLD_CHUNKS * WORLD_CHUNKS * MAX_PROPS_PER_CHUNK)

static void Join(char *dest, int size, const char *dir, const char *name)
{
    snprintf(dest, (size_t)size, "%s/%s", dir, name);
}

/* clock() e non clock_gettime(): con -std=c99 quest'ultima richiede macro
 * POSIX, e il baker gira su un solo core, dove il tempo di CPU e' il tempo. */
static double Now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* ------------------------------------------------------------------------ */
/*  SCRITTURA                                                              */
/* ------------------------------------------------------------------------ */

static bool WriteGrid(const char *path, uint32_t magic, const void *samples,
                      int sampleSize, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) { perror(path); return false; }

    WorldGridHeader hd = { magic, WORLD_FMT_VERSION, (uint32_t)w, (uint32_t)h };
    size_t count = (size_t)w * (size_t)h;
    bool ok = fwrite(&hd, sizeof(hd), 1, f) == 1 &&
              fwrite(samples, (size_t)sampleSize, count, f) == count;
    if (!ok) perror(path);
    fclose(f);
    return ok;
}

/* props.bin: intestazione, directory, record. La capienza di ogni chunk e' il
 * suo conteggio piu' WORLD_PROP_SLACK, cosi' le prime aggiunte di un editor
 * entrano dove sono senza accodare in fondo al file. */
static bool WriteProps(const char *path, const WorldChunkEntry *dir,
                       const WorldPropRec *recs, uint32_t recCount)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) { perror(path); return false; }

    const uint32_t chunks = (uint32_t)(WORLD_CHUNKS * WORLD_CHUNKS);
    WorldPropsHeader hd;
    hd.magic      = WORLD_MAGIC_PROPS;
    hd.version    = WORLD_FMT_VERSION;
    hd.chunkCount = chunks;
    hd.recordSize = (uint32_t)sizeof(WorldPropRec);
    hd.recordCount = recCount;
    hd.dataOffset = (uint32_t)(sizeof(WorldPropsHeader) +
                               sizeof(WorldChunkEntry) * chunks);

    bool ok = fwrite(&hd, sizeof(hd), 1, f) == 1 &&
              fwrite(dir, sizeof(WorldChunkEntry), chunks, f) == chunks &&
              (recCount == 0 ||
               fwrite(recs, sizeof(WorldPropRec), recCount, f) == recCount);
    if (!ok) perror(path);
    fclose(f);
    return ok;
}

static bool WriteManifest(const char *path, const WorldGen *g,
                          float minH, float maxH, uint32_t propCount,
                          double bakeSeconds)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) { perror(path); return false; }

    fprintf(f,
        "# ============================================================================\n"
        "#  manifest.txt - carta d'identita' del mondo cotto.\n"
        "#\n"
        "#  Lo ha scritto tools/baker in %.2f s. Il seme non serve piu' al gioco: e'\n"
        "#  qui perche' si sappia da dove viene questo mondo e perche' un salvataggio\n"
        "#  fatto in un altro mondo si possa riconoscere e rifiutare.\n"
        "#\n"
        "#  Le prime cinque chiavi devono combaciare con config.h: se non combaciano il\n"
        "#  gioco non parte, invece di disegnare un terreno spostato di metri.\n"
        "# ============================================================================\n"
        "\n"
        "[mondo]\n"
        "versione        = %d\n"
        "seme            = %u\n"
        "lato_metri      = %.1f\n"
        "chunk           = %d\n"
        "passo_griglia   = %.1f\n"
        "campioni        = %d\n"
        "\n"
        "# Informativi: il gioco li legge ma non li usa per decidere.\n"
        "quota_minima    = %.2f\n"
        "quota_massima   = %.2f\n"
        "prop            = %u\n"
        "generato        = " GAME_NAME " " GAME_VERSION " baker\n",
        bakeSeconds, WORLD_FMT_VERSION, g->seed, (double)WORLD_SIZE, WORLD_CHUNKS,
        (double)WORLD_GRID_STEP, WORLD_GRID, (double)minH, (double)maxH, propCount);

    fclose(f);
    return true;
}

static bool WriteSpawns(const char *path, const WorldGen *g)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) { perror(path); return false; }

    fprintf(f,
        "# ============================================================================\n"
        "#  spawns.txt - villaggi, abitanti, cripta e punto di partenza.\n"
        "#\n"
        "#  Questo file e' fatto per essere modificato a mano: e' il mondo autoriale.\n"
        "#  Spostare un villaggio, aggiungere una guardia o cambiare il punto di\n"
        "#  partenza si fa qui, e si vede al riavvio senza ricuocere niente.\n"
        "#\n"
        "#  'posizione' accetta due coordinate (x z) o tre (x y z): con due, la quota\n"
        "#  la prende dal terreno cotto, che e' quasi sempre cio' che si vuole.\n"
        "#  'npc = <tipo> <x> <z>': il tipo e' un identificatore di\n"
        "#  assets/data/entities.txt, non un numero.\n"
        "#\n"
        "#  ATTENZIONE: le quote del terreno dentro il raggio di un villaggio sono\n"
        "#  spianate in height.bin. Spostare un villaggio qui NON rispiana il terreno:\n"
        "#  per quello serve ricuocere (make mondo-forza).\n"
        "# ============================================================================\n");

    for (int i = 0; i < g->townCount; i++) {
        const Town *t = &g->towns[i];
        fprintf(f, "\n[villaggio %d]\n", i);
        fprintf(f, "nome            = %s\n", t->name);
        fprintf(f, "posizione       = %.2f %.2f %.2f\n", t->pos.x, t->pos.y, t->pos.z);
        fprintf(f, "raggio          = %.1f\n", t->radius);
        fprintf(f, "quota_base      = %.2f\n", t->baseHeight);

        NpcSpawn npcs[MAX_WORLD_NPCS];
        int n = GenTownNpcs(g, i, npcs, MAX_WORLD_NPCS);
        for (int k = 0; k < n; k++)
            fprintf(f, "npc             = %-9s %.2f %.2f\n",
                    npcs[k].type, npcs[k].x, npcs[k].z);
    }

    fprintf(f, "\n[cripta]\n");
    fprintf(f, "posizione       = %.2f %.2f %.2f\n",
            g->cryptPos.x, g->cryptPos.y, g->cryptPos.z);

    Vector3 start = GenPlayerStart(g);
    fprintf(f, "\n[inizio]\n");
    fprintf(f, "posizione       = %.2f %.2f %.2f\n", start.x, start.y, start.z);

    fclose(f);
    return true;
}

/* La grana del terreno nasceva dal rumore a ogni avvio. Ora che il rumore non
 * sta piu' nel gioco, si cuoce: e' l'unica texture procedurale del progetto. */
static bool WriteGrain(const char *path, unsigned int seed, int size)
{
    Image img = GenImageColor(size, size, WHITE);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float n = NoiseFBM(seed + 55u, (float)x * 0.09f, (float)y * 0.09f,
                               4, 2.0f, 0.5f);
            unsigned char v = (unsigned char)(190 + n * 65.0f);
            ImageDrawPixel(&img, x, y, (Color){ v, v, v, 255 });
        }
    }
    bool ok = ExportImage(img, path);
    UnloadImage(img);
    if (!ok) fprintf(stderr, "%s: scrittura fallita\n", path);
    return ok;
}

/* ------------------------------------------------------------------------ */
/*  BAKE                                                                    */
/* ------------------------------------------------------------------------ */

static int Bake(unsigned int seed, const char *outDir, const char *heightmapPath)
{
    double t0 = Now();

    /* Heightmap esterna: prende il posto del rumore. Il seme continua a decidere
     * tutto il resto (umidita', villaggi, prop), quindi serve anche con la
     * heightmap. */
    Image hm = { 0 };
    if (heightmapPath != NULL) {
        if (!FileExists(heightmapPath)) {
            fprintf(stderr, "%s: non esiste\n", heightmapPath);
            return 1;
        }
        hm = LoadImage(heightmapPath);
        if (hm.data == NULL) {
            fprintf(stderr, "%s: non si legge come immagine\n", heightmapPath);
            return 1;
        }
        ImageFormat(&hm, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        printf("heightmap: %s (%dx%d)\n", heightmapPath, hm.width, hm.height);
    }

    WorldGen gen;
    GenInit(&gen, seed, hm.data != NULL ? &hm : NULL);
    printf("villaggi: %d", gen.townCount);
    for (int i = 0; i < gen.townCount; i++)
        printf("%s %s (%.0f, %.0f)", i ? "," : "  ",
               gen.towns[i].name, gen.towns[i].pos.x, gen.towns[i].pos.z);
    printf("\ncripta:   (%.0f, %.0f, %.0f)\n",
           gen.cryptPos.x, gen.cryptPos.y, gen.cryptPos.z);

    /* --- quote e biomi --------------------------------------------------- */
    size_t samples = (size_t)WORLD_GRID * (size_t)WORLD_GRID;
    uint16_t *height = (uint16_t *)malloc(samples * sizeof(uint16_t));
    uint8_t  *biome  = (uint8_t  *)malloc(samples);
    if (height == NULL || biome == NULL) {
        fprintf(stderr, "memoria insufficiente per %zu campioni\n", samples);
        return 1;
    }

    float minH = 1e9f, maxH = -1e9f;
    int biomeHist[BIOME_COUNT] = { 0 };
    double tGrid = Now();

    for (int gz = 0; gz < WORLD_GRID; gz++) {
        for (int gx = 0; gx < WORLD_GRID; gx++) {
            float x = (float)gx * WORLD_GRID_STEP;
            float z = (float)gz * WORLD_GRID_STEP;
            float h = GenHeight(&gen, x, z);
            Biome b = GenBiomeAtHeight(&gen, x, z, h);

            size_t i = (size_t)gz * WORLD_GRID + (size_t)gx;
            height[i] = WorldEncodeHeight(h);
            biome[i]  = (uint8_t)b;

            if (h < minH) minH = h;
            if (h > maxH) maxH = h;
            biomeHist[b]++;
        }
        if (gz % 256 == 0) { printf("\rquote e biomi: %3d%%", gz * 100 / WORLD_GRID); fflush(stdout); }
    }
    printf("\rquote e biomi: 100%%  (%.2f s, da %.1f a %.1f m)\n",
           Now() - tGrid, (double)minH, (double)maxH);

    /* --- prop ------------------------------------------------------------ */
    double tProps = Now();
    const uint32_t chunks = (uint32_t)(WORLD_CHUNKS * WORLD_CHUNKS);
    WorldChunkEntry *dir = (WorldChunkEntry *)calloc(chunks, sizeof(WorldChunkEntry));
    WorldPropRec    *recs = (WorldPropRec *)malloc((size_t)BAKER_MAX_PROPS *
                                                   sizeof(WorldPropRec));
    if (dir == NULL || recs == NULL) {
        fprintf(stderr, "memoria insufficiente per i prop\n");
        return 1;
    }

    Prop chunkProps[MAX_PROPS_PER_CHUNK];
    uint32_t used = 0, total = 0, fullChunks = 0, maxInChunk = 0;
    int propHist[PROP_COUNT] = { 0 };

    for (int cz = 0; cz < WORLD_CHUNKS; cz++) {
        for (int cx = 0; cx < WORLD_CHUNKS; cx++) {
            int n = GenChunkProps(&gen, cx, cz, chunkProps, MAX_PROPS_PER_CHUNK);
            if (n == MAX_PROPS_PER_CHUNK) fullChunks++;
            if ((uint32_t)n > maxInChunk) maxInChunk = (uint32_t)n;

            WorldChunkEntry *e = &dir[(size_t)cz * WORLD_CHUNKS + cx];
            e->first    = used;
            e->count    = (uint16_t)n;
            e->capacity = (uint16_t)(n + WORLD_PROP_SLACK);
            if (e->capacity > MAX_PROPS_PER_CHUNK)
                e->capacity = MAX_PROPS_PER_CHUNK;
            if (e->capacity < e->count) e->capacity = e->count;

            for (int i = 0; i < n; i++) {
                const Prop *p = &chunkProps[i];
                WorldPropRec *r = &recs[used + (uint32_t)i];
                r->lx     = WorldEncodeLocal(p->pos.x - (float)cx * CHUNK_SIZE);
                r->lz     = WorldEncodeLocal(p->pos.z - (float)cz * CHUNK_SIZE);
                r->y      = WorldEncodeHeight(p->pos.y);
                r->type   = (uint8_t)p->type;
                r->scale  = WorldEncodeScale(p->scale);
                r->rot    = WorldEncodeRot(p->rot);
                r->radius = WorldEncodeRadius(p->radius);
                propHist[p->type]++;
            }
            /* I posti di riserva restano azzerati: se qualcuno li leggesse per
             * errore vedrebbe un albero di scala zero, non memoria casuale. */
            memset(&recs[used + (uint32_t)n], 0,
                   (size_t)(e->capacity - n) * sizeof(WorldPropRec));

            used  += e->capacity;
            total += (uint32_t)n;
        }
        if (cz % 8 == 0) { printf("\rprop: %3d%%", cz * 100 / WORLD_CHUNKS); fflush(stdout); }
    }
    printf("\rprop: 100%%  (%.2f s, %u istanze in %u posti)\n",
           Now() - tProps, total, used);
    if (fullChunks > 0)
        printf("  ATTENZIONE: %u chunk hanno saturato MAX_PROPS_PER_CHUNK (%d): "
               "dei prop sono stati scartati.\n", fullChunks, MAX_PROPS_PER_CHUNK);

    /* --- scrittura ------------------------------------------------------- */
    char path[256];
    if (!DirectoryExists(outDir)) MakeDirectory(outDir);

    Join(path, sizeof(path), outDir, WORLD_HEIGHT);
    if (!WriteGrid(path, WORLD_MAGIC_HEIGHT, height, (int)sizeof(uint16_t),
                   WORLD_GRID, WORLD_GRID)) return 1;
    Join(path, sizeof(path), outDir, WORLD_BIOME);
    if (!WriteGrid(path, WORLD_MAGIC_BIOME, biome, 1, WORLD_GRID, WORLD_GRID))
        return 1;
    Join(path, sizeof(path), outDir, WORLD_PROPS);
    if (!WriteProps(path, dir, recs, used)) return 1;
    Join(path, sizeof(path), outDir, WORLD_SPAWNS);
    if (!WriteSpawns(path, &gen)) return 1;
    Join(path, sizeof(path), outDir, WORLD_GRAIN);
    if (!WriteGrain(path, seed, 256)) return 1;
    Join(path, sizeof(path), outDir, WORLD_MANIFEST);
    if (!WriteManifest(path, &gen, minH, maxH, total, Now() - t0)) return 1;

    /* --- resoconto ------------------------------------------------------- */
    static const char *biomeName[BIOME_COUNT] = {
        "oceano", "spiaggia", "pianura", "foresta", "colline", "montagna", "nevi"
    };
    static const char *propName[PROP_COUNT] = {
        "albero", "pino", "roccia", "cespuglio", "erba", "casa", "torre", "cripta"
    };
    printf("\nbiomi:");
    for (int b = 0; b < BIOME_COUNT; b++)
        printf(" %s %.1f%%", biomeName[b], 100.0 * biomeHist[b] / (double)samples);
    printf("\nprop: ");
    for (int t = 0; t < PROP_COUNT; t++)
        printf(" %s %d", propName[t], propHist[t]);
    printf("\n\ncotto in %.2f s -> %s\n", Now() - t0, outDir);

    free(height);
    free(biome);
    free(dir);
    free(recs);
    if (hm.data != NULL) UnloadImage(hm);
    return 0;
}

/* ------------------------------------------------------------------------ */
/*  VERIFICA                                                                */
/* ------------------------------------------------------------------------ */

/* Ricarica il mondo cotto con lo stesso codice del gioco e lo confronta con
 * quello che il generatore produce dal seme scritto nel manifest. E' il
 * controllo che chiude la fase 3: se questi numeri sono a zero, il mondo
 * caricato e' il mondo generato entro l'errore di quantizzazione. */
static int Verify(const char *dir)
{
    WorldIo io;
    DataProblemReset();
    if (!WorldIoLoad(&io, dir)) {
        fprintf(stderr, "verifica interrotta: %d problemi nel mondo cotto.\n",
                DataProblemCount());
        return 1;
    }

    /* La verifica confronta col rumore: un mondo cotto da una heightmap
     * mostrera' differenze nelle quote, ed e' corretto che le mostri. */
    WorldGen gen;
    GenInit(&gen, io.seed, NULL);

    /* Quote: tolleranza mezzo centimetro, l'arrotondamento di uint16. */
    double worstH = 0.0;
    int    badH = 0, badB = 0;
    const int STRIDE = 7;            /* ~85.000 campioni: bastano e ci mette un attimo */

    for (int gz = 0; gz < WORLD_GRID; gz += STRIDE) {
        for (int gx = 0; gx < WORLD_GRID; gx += STRIDE) {
            float x = (float)gx * WORLD_GRID_STEP;
            float z = (float)gz * WORLD_GRID_STEP;
            float want = GenHeight(&gen, x, z);
            float got  = WorldIoHeight(&io, x, z);
            double d = fabs((double)want - (double)got);
            if (d > worstH) worstH = d;
            if (d > 0.006) badH++;

            if (WorldIoBiome(&io, x, z) != GenBiomeAtHeight(&gen, x, z, want)) badB++;
        }
    }

    /* Prop: conteggio, posizione e tutto cio' che se ne vede - tipo, scala,
     * rotazione, raggio di collisione. Confrontare solo le posizioni lascerebbe
     * passare un albero grande il doppio nel punto giusto. */
    int badCount = 0, badPos = 0, badLook = 0, checkedChunks = 0, checkedProps = 0;
    double worstP = 0.0, worstS = 0.0, worstR = 0.0, worstRad = 0.0;
    Prop want[MAX_PROPS_PER_CHUNK], got[MAX_PROPS_PER_CHUNK];

    for (int cz = 0; cz < WORLD_CHUNKS; cz += 5) {
        for (int cx = 0; cx < WORLD_CHUNKS; cx += 5) {
            int nw = GenChunkProps(&gen, cx, cz, want, MAX_PROPS_PER_CHUNK);
            int ng = WorldIoChunkProps(&io, cx, cz, got, MAX_PROPS_PER_CHUNK);
            checkedChunks++;
            if (nw != ng) { badCount++; continue; }
            for (int i = 0; i < nw; i++) {
                double d = fabs(want[i].pos.x - got[i].pos.x) +
                           fabs(want[i].pos.y - got[i].pos.y) +
                           fabs(want[i].pos.z - got[i].pos.z);
                if (d > worstP) worstP = d;
                if (want[i].type != got[i].type || d > 0.02) badPos++;

                double ds = fabs(want[i].scale - got[i].scale);
                double dr = fabs(want[i].rot   - got[i].rot);
                if (dr > 180.0) dr = 360.0 - dr;        /* 359,9 e 0,1 distano 0,2 */
                double dd = fabs(want[i].radius - got[i].radius);
                if (ds > worstS)   worstS = ds;
                if (dr > worstR)   worstR = dr;
                if (dd > worstRad) worstRad = dd;
                /* Tolleranze: 1/128 di scala, mezzo passo di rotazione, 2,5 cm
                 * di raggio - la meta' del gradino di ciascuna codifica. */
                if (ds > 0.008 || dr > 0.71 || dd > 0.025) badLook++;
                checkedProps++;
            }
        }
    }

    /* Villaggi, cripta e inizio: qui il confronto e' informativo, perche'
     * spawns.txt e' fatto per essere modificato a mano. */
    int movedTowns = 0;
    if (io.townCount != gen.townCount) movedTowns = -1;
    else for (int i = 0; i < io.townCount; i++)
        if (fabsf(io.towns[i].pos.x - gen.towns[i].pos.x) > 0.02f ||
            fabsf(io.towns[i].pos.z - gen.towns[i].pos.z) > 0.02f) movedTowns++;

    printf("mondo:    %s (seme %u)\n", dir, io.seed);
    printf("quote:    %d campioni oltre tolleranza su %d, errore massimo %.4f m\n",
           badH, (WORLD_GRID / STRIDE + 1) * (WORLD_GRID / STRIDE + 1), worstH);
    printf("biomi:    %d campioni diversi\n", badB);
    printf("prop:     %d chunk con conteggio diverso su %d, %d posizioni oltre "
           "tolleranza su %d, errore massimo %.4f m\n",
           badCount, checkedChunks, badPos, checkedProps, worstP);
    printf("aspetto:  %d prop diversi da vedere; errore massimo scala %.4f, "
           "rotazione %.3f gradi, raggio %.4f m\n",
           badLook, worstS, worstR, worstRad);
    if (movedTowns < 0)
        printf("villaggi: spawns.txt ne ha %d, il generatore %d (modificato a mano?)\n",
               io.townCount, gen.townCount);
    else
        printf("villaggi: %d spostati rispetto al generatore (a mano, se non zero)\n",
               movedTowns);

    WorldIoFree(&io);

    bool ok = (badH == 0 && badB == 0 && badCount == 0 && badPos == 0 && badLook == 0);
    printf("\n%s\n", ok ? "il mondo caricato coincide con quello generato."
                        : "DIFFERENZE: il mondo caricato non coincide con quello generato.");
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    unsigned int seed = BAKER_SEED_DEFAULT;
    const char  *out  = WORLD_DIR;
    const char  *heightmap = NULL;
    bool verify = false, force = false;

    SetTraceLogLevel(LOG_WARNING);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seme") == 0 && i + 1 < argc)
            seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "--heightmap") == 0 && i + 1 < argc)
            heightmap = argv[++i];
        else if (strcmp(argv[i], "--verifica") == 0) verify = true;
        else if (strcmp(argv[i], "--forza") == 0)    force = true;
        else {
            fprintf(stderr,
                "uso: %s [--seme N] [--out cartella] [--heightmap file.png]\n"
                "            [--forza] [--verifica]\n"
                "  --seme       seme d'origine (predefinito %u)\n"
                "  --out        dove scrivere (predefinito %s)\n"
                "  --heightmap  PNG in scala di grigi al posto del rumore\n"
                "  --forza      sovrascrive un mondo esistente\n"
                "  --verifica   confronta il mondo cotto con quello generato\n",
                argv[0], BAKER_SEED_DEFAULT, WORLD_DIR);
            return 2;
        }
    }

    if (verify) return Verify(out);

    char manifest[256];
    Join(manifest, sizeof(manifest), out, WORLD_MANIFEST);
    if (FileExists(manifest) && !force) {
        fprintf(stderr,
            "%s esiste gia'.\n"
            "Ricuocere cancella le modifiche fatte a mano al mondo (spawns.txt\n"
            "compreso). Se e' cio' che vuoi: %s --forza\n", manifest, argv[0]);
        return 1;
    }

    return Bake(seed, out, heightmap);
}
