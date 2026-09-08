#include "world.h"
#include "rlgl.h"
#include "light.h"
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
/* La tabella dichiara QUANTO DEVE ESSERE GRANDE l'oggetto in metri, non di
 * quanto va moltiplicato il file. La scala si ricava al caricamento
 * dall'ingombro vero del modello.
 *
 * Prima erano costanti tarate a mano sul pacchetto Kenney - 3,54 perche' quel
 * sasso e' largo 0,62 m - e cambiare pacchetto voleva dire rifare i conti a
 * mano, con un errore che non da' nessun avviso: un albero alto tre volte
 * tanto o un masso che sprofonda. Le misure di riferimento restano quelle di
 * prima, e con i file di Kenney escono le stesse scale.
 *
 * 'perAltezza' dice quale dimensione conta: un albero si misura in altezza,
 * un sasso in larghezza. */
static const struct { const char *file; float voluto; bool perAltezza; }
gExtProp[PROP_COUNT] = {
    [PROP_TREE] = { "assets/models/tree.glb",            6.5f, true  },
    [PROP_PINE] = { "assets/models/pine.glb",            6.8f, true  },
    [PROP_ROCK] = { "assets/models/rock.glb",            2.2f, false },
    [PROP_BUSH] = { "assets/models/bush.glb",            1.4f, false },
    /* L'erba si misura in LARGHEZZA: le piante da prato del catalogo sono
     * rosette appoggiate a terra - la celidonia e' 0,28 larga per 0,19 alta -
     * e tararle sull'altezza le sgonfierebbe di traverso. I 0,9 m di prima
     * erano l'altezza del ciuffo stilizzato, non una misura. */
    [PROP_HERB] = { "assets/models/herb.glb",            0.6f, false },
    /* La cripta e' un TUMULO: un anello di massi, non un edificio. Il file e'
     * il set di sei massi muschiati, e la taglia e' quella di UN masso - 2,8 m
     * sul lato XZ maggiore, come i sassi sparsi, perche' un masso si misura in
     * larghezza e non in altezza.
     *
     * Per tornare alla cripta del kit non basta rimettere il file: vanno
     * rimessi anche questi due numeri (5,0 e true), o il modello uscirebbe alto
     * meno di tre metri. */
    [PROP_CRYPT]= { "assets/models/crypt.glb",           2.8f, false },
};

/* Il file puo' essere .glb o .gltf: i kit spediscono il primo, Poly Haven il
 * secondo con il .bin e le texture accanto. Si prova quello dichiarato e poi
 * l'altra estensione, cosi' sostituire un asset non richiede di ricompilare. */
static const char *TrovaModello(const char *file, char *buf, int n)
{
    if (FileExists(file)) return file;

    const char *punto = strrchr(file, '.');
    if (punto == NULL) return NULL;
    const char *altra = (strcmp(punto, ".glb") == 0) ? ".gltf" : ".glb";
    snprintf(buf, (size_t)n, "%.*s%s", (int)(punto - file), file, altra);
    return FileExists(buf) ? buf : NULL;
}

/* --- Edifici modulari ----------------------------------------------------
 * I pezzi vengono da kit diversi, quindi da cartelle diverse: ognuno porta il
 * suo Textures/colormap.png e i file hanno lo stesso nome.
 * La cella e' l'unita' dei kit di Kenney: qui vale BUILD_CELL metri.
 *
 * Un pezzo e' un file, O UN PEZZO DENTRO UN FILE. Kenney spedisce un muro per
 * file; Poly Haven spedisce venti pezzi di fortezza in uno solo, 45 primitive,
 * e il raggruppamento delle mesh li separa gia' da se'.
 *
 * 'pezzo' e' l'indice del gruppo, -1 per "tutto il file". L'indice e'
 * riproducibile ma NON stabile nel tempo: se il catalogo ricuoce il file e
 * riordina i pezzi, il 12 diventa un muro e non c'e' nessun avviso. Per questo
 * la riga dichiara anche 'atteso', l'ingombro che ci si aspetta di trovare.
 *
 * 'voluto' e' la taglia in metri, come in gExtProp: la scala esce
 * dall'ingombro MISURATO, non da una costante tarata a mano. Vale solo per i
 * pezzi indicizzati - quelli dei kit stanno sulla griglia di BUILD_CELL, e
 * hanno 'voluto' a zero. */
static const struct {
    const char *file;
    int         pezzo;        /* -1 = tutto il file           */
    Vector3     atteso;       /* ingombro del pezzo, in metri */
    float       voluto;       /* taglia dichiarata, in metri  */
    bool        perAltezza;
} BUILD_FILES[BUILD_PART_COUNT] = {
    [BUILD_WALL]        = { "assets/models/town/wall.glb",                       -1, { 0 }, 0.0f, false },
    [BUILD_DOOR]        = { "assets/models/town/wall-doorway-round.glb",         -1, { 0 }, 0.0f, false },
    [BUILD_WINDOW]      = { "assets/models/town/wall-window-small.glb",          -1, { 0 }, 0.0f, false },
    [BUILD_ROOF]        = { "assets/models/town/roof-gable.glb",                 -1, { 0 }, 0.0f, false },
    [BUILD_FLOOR]       = { "assets/models/town/planks.glb",                     -1, { 0 }, 0.0f, false },
    [BUILD_STAIRS]      = { "assets/models/town/stairs-wide-wood.glb",           -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_BASE]  = { "assets/models/castle/tower-square-base.glb",        -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_MID]   = { "assets/models/castle/tower-square-mid-windows.glb", -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_TOP]   = { "assets/models/castle/tower-square-top.glb",         -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_ROOF]  = { "assets/models/castle/tower-square-top-roof.glb",    -1, { 0 }, 0.0f, false },

    /* tower_round di modular_fort_01, misurato il 2026-09-08: gruppo 12 dei
     * venti, 2 mesh, 4.544 vertici, base a Y = 0. La taglia dichiarata e'
     * l'altezza vera dell'asset, quindi la scala esce 1,00 - il numero e'
     * dichiarato lo stesso, e non sottinteso. */
    [BUILD_KEEP]        = { "assets/models/fort/modular_fort_01.gltf",           12,
                            { 15.84f, 13.50f, 15.84f }, 13.50f, true },

    /* gothic_statue, misurata il 2026-09-08: 1,48 x 1,74 x 1,56 m, 1 mesh,
     * 23.314 vertici, un materiale PBR. E' un file intero - pezzo -1 - ma con
     * una taglia dichiarata, perche' non sta sulla griglia di BUILD_CELL: e'
     * l'unico oggetto del catalogo che dica "tomba" invece di "sasso".
     *
     * TRE METRI e non i suoi 1,74 di scansione. L'ingombro non dice se il volume
     * e' pieno: questa e' una filigrana, quasi tutta aria, e alla sua taglia
     * vera si perde nell'erba come un rametto scuro. Visto, non dedotto. */
    [BUILD_STATUE]      = { "assets/models/statua.glb",                        -1,
                            { 0 }, 3.0f, true },
};

/* Quanto puo' scostarsi l'ingombro misurato da quello dichiarato, per lato.
 * Un quinto: largo abbastanza da reggere una ricottura che cambia l'asset di
 * poco, stretto abbastanza da distinguere la torre tonda da ogni altro pezzo
 * del file - il piu' vicino per altezza e' un bastione da 8,61 m, che sta il
 * 36% sotto. */
#define BUILD_TOLLERANZA  0.20f

/* Metri per cella. 3x2 celle fanno una casa di 7,8 x 5,2 m con i muri alti
 * 2,6: le stesse dimensioni della scatola procedurale che sostituisce. */
#define BUILD_CELL   2.6f

/* Che materiale va su ogni pezzo, con che passo e con che modo di proiezione.
 * I pezzi dei kit non hanno UV utilizzabili - wall.glb ha 64 vertici e QUATTRO
 * sole coppie UV distinte, dentro un riquadro di 0,375x0,35 della tavolozza -
 * quindi la texture si ricava dalla posizione e non dalle UV.
 *
 * Il passo e' per INSIEME e non per pezzo: le assi di un muro e quelle di una
 * finestra devono avere lo stesso, o la finestra si stacca dalla parete.
 *
 * Il tetto e' l'unico a modo 2: le sue falde hanno la normale a 45 gradi, e
 * con l'asse dominante nascerebbe una cucitura a meta' falda.
 *
 * 'uvVere' dice che il pezzo campiona le PROPRIE UV e non va proiettato. Non
 * basta lasciare la riga vuota: una riga mancante e' quasi sempre una riga
 * dimenticata, e viene gridata. Qui l'assenza di materiale proiettato e' la
 * scelta giusta - i pezzi di Poly Haven hanno UV vere e portano le loro mappe
 * PBR - quindi va detta, non sottintesa. */
typedef struct { const char *nome; float tile; int mode; bool uvVere; } BuildMat;

static const BuildMat gBuildMat[BUILD_PART_COUNT] = {
    [BUILD_WALL]        = { "legno_scuro", 2.0f, 1, false },
    [BUILD_DOOR]        = { "legno_scuro", 2.0f, 1, false },
    [BUILD_WINDOW]      = { "legno_scuro", 2.0f, 1, false },
    [BUILD_ROOF]        = { "tetto_legno", 1.5f, 2, false },
    [BUILD_FLOOR]       = { "assito",      2.0f, 1, false },
    [BUILD_STAIRS]      = { "assito",      2.0f, 1, false },
    [BUILD_TOWER_BASE]  = { "pietra",      2.5f, 1, false },
    [BUILD_TOWER_MID]   = { "pietra",      2.5f, 1, false },
    [BUILD_TOWER_TOP]   = { "pietra",      2.5f, 1, false },
    [BUILD_TOWER_ROOF]  = { "pietra",      2.5f, 1, false },
    [BUILD_KEEP]        = { NULL,          0.0f, 0, true  },
    [BUILD_STATUE]      = { NULL,          0.0f, 0, true  },
};

/* Sostituisce la texture di una mappa su TUTTI i materiali di un modello,
 * scaricando prima quella che sta per essere persa.
 *
 * Senza lo scarico l'id vecchio non e' piu' raggiungibile da nessuno:
 * UnloadModel() liberera' la texture che la mappa contiene ALLORA, cioe' quella
 * nuova, e la tavolozza del kit resta in memoria sulla scheda a ogni
 * caricamento del mondo.
 *
 * Due esclusioni. La texture di riposo di rlgl e' condivisa da tutto il motore
 * e non appartiene al modello. E un id gia' incontrato non si scarica due
 * volte: due materiali dello stesso .glb possono puntare alla stessa texture,
 * e la seconda UnloadTexture() colpirebbe un id ormai riassegnato a qualcun
 * altro. */
static void SostituisciMappa(Material *mats, int count, int mappa, Texture2D nuova)
{
    for (int k = 0; k < count; k++) {
        unsigned int id = mats[k].maps[mappa].texture.id;
        if (id == 0 || id == nuova.id || id == rlGetTextureIdDefault()) continue;

        bool gia = false;
        for (int j = 0; j < k; j++)
            if (mats[j].maps[mappa].texture.id == id) { gia = true; break; }
        if (!gia) UnloadTexture(mats[k].maps[mappa].texture);
    }
    for (int k = 0; k < count; k++) mats[k].maps[mappa].texture = nuova;
}

/* Porta all'origine le sole mesh del pezzo scelto, sulla CPU.
 *
 * Non tocca la scheda apposta: chi chiama fa UpdateMeshBuffer() sulle stesse
 * mesh, e cosi' questa parte - che e' quella dove si sbaglia - resta provabile
 * senza un contesto grafico.
 *
 * Solo quelle del pezzo scelto: gli altri diciannove pezzi del file restano
 * dove il catalogo li ha messi. Spostarli non si vedrebbe oggi, perche' non li
 * disegna nessuno, e si vedrebbe il giorno che se ne usa un secondo.
 *
 * Torna quante mesh ha spostato: chi chiama sa cosi' su quali fare il
 * caricamento sulla scheda, e una mesh senza vertici non finisce dentro
 * UpdateMeshBuffer(), che deferenzia anche vboId. */
static int RicentraPezzo(Model *m, const int *idx, const MeshGroup *g, Vector3 o)
{
    if (m == NULL || idx == NULL || g == NULL) return 0;

    int mosse = 0;
    for (int k = 0; k < g->count; k++) {
        Mesh *me = &m->meshes[idx[g->first + k]];
        if (me->vertices == NULL) continue;
        MeshGroupRecenter(me->vertices, me->vertexCount, o);
        mosse++;
    }
    return mosse;
}

/* Carica il modello di un pezzo, riusando quello di un pezzo gia' caricato che
 * nomina lo STESSO file.
 *
 * Serve perche' un file puo' contenere venti pezzi: senza, usarne due vorrebbe
 * dire tenerne in memoria quaranta. Oggi il forte da' un pezzo solo e la cache
 * non scatta mai; la cripta della domanda E la fara' scattare, e allora sara'
 * gia' li'.
 *
 * Chi riusa NON possiede: buildOwned resta false, e WorldUnload lo salta. */
static bool CaricaPezzo(World *w, int i)
{
    for (int k = 0; k < i; k++)
        if (w->buildLoaded[k] &&
            strcmp(BUILD_FILES[k].file, BUILD_FILES[i].file) == 0) {
            w->buildPart[i]  = w->buildPart[k];
            w->buildOwned[i] = false;
            return true;
        }

    char alt[256];
    const char *file = TrovaModello(BUILD_FILES[i].file, alt, (int)sizeof alt);
    if (file == NULL) return false;

    Model m = LoadModel(file);
    if (m.meshCount == 0) {
        TraceLog(LOG_WARNING, "WORLD: %s non caricato", file);
        UnloadModel(m);
        return false;
    }
    w->buildPart[i]  = m;
    w->buildOwned[i] = true;
    return true;
}

/* Il pezzo k del file X: si separano i gruppi, si prende il k-esimo, si
 * verifica che somigli a quello dichiarato, lo si porta sull'origine e gli si
 * fa il lotto con le sole sue mesh.
 *
 * Torna false quando il pezzo non c'e' o non e' quello atteso. Non e' un
 * errore: il chiamante ripiega sui pezzi del kit, che e' molto meglio di un
 * muro messo dove va una torre - lo stesso ragionamento di TroppiVertici(). */
static bool PreparaPezzo(World *w, int i)
{
    Model *m  = &w->buildPart[i];
    int    nm = m->meshCount;

    BoundingBox *bb  = (BoundingBox *)MemAlloc((unsigned int)(nm * (int)sizeof(BoundingBox)));
    int         *idx = (int *)MemAlloc((unsigned int)(nm * (int)sizeof(int)));
    MeshGroup   *gr  = (MeshGroup *)MemAlloc((unsigned int)(nm * (int)sizeof(MeshGroup)));
    if (bb == NULL || idx == NULL || gr == NULL) {
        MemFree(bb); MemFree(idx); MemFree(gr);
        return false;
    }

    for (int k = 0; k < nm; k++) bb[k] = GetMeshBoundingBox(m->meshes[k]);
    int ng = MeshGroupSplit(bb, nm, idx, gr, nm);
    MemFree(bb);

    const MeshGroup *g = MeshGroupPick(gr, ng, BUILD_FILES[i].pezzo);
    if (g == NULL) {
        TraceLog(LOG_WARNING, "WORLD: %s non ha il pezzo %d (ne ha %d)",
                 BUILD_FILES[i].file, BUILD_FILES[i].pezzo, ng);
        MemFree(idx); MemFree(gr);
        return false;
    }

    /* L'indice e' posizionale: se il catalogo ricuoce il file, il 12 diventa un
     * muro e non se ne accorge nessuno. Qui si guarda cosa si e' trovato. */
    if (!MeshGroupSomiglia(g, BUILD_FILES[i].atteso, BUILD_TOLLERANZA)) {
        TraceLog(LOG_WARNING,
                 "WORLD: %s pezzo %d misura %.2fx%.2fx%.2f, atteso %.2fx%.2fx%.2f: si ripiega",
                 BUILD_FILES[i].file, BUILD_FILES[i].pezzo,
                 (double)(g->box.max.x - g->box.min.x),
                 (double)(g->box.max.y - g->box.min.y),
                 (double)(g->box.max.z - g->box.min.z),
                 (double)BUILD_FILES[i].atteso.x,
                 (double)BUILD_FILES[i].atteso.y,
                 (double)BUILD_FILES[i].atteso.z);
        MemFree(idx); MemFree(gr);
        return false;
    }

    /* Il pezzo porta cucito l'offset che lo mette in fila con gli altri
     * diciannove: si toglie qui, una volta, invece che a ogni fotogramma. Poi
     * si scrive sulla scheda quello che si e' mosso. */
    MeshGroup scelto = *g;
    Vector3   o      = MeshGroupOrigin(&scelto);
    RicentraPezzo(m, idx, &scelto, o);
    for (int k = 0; k < scelto.count; k++) {
        Mesh *me = &m->meshes[idx[scelto.first + k]];
        if (me->vertices == NULL) continue;
        UpdateMeshBuffer(*me, 0, me->vertices,
                         me->vertexCount * 3 * (int)sizeof(float), 0);
    }
    scelto.box.min = Vector3Subtract(scelto.box.min, o);
    scelto.box.max = Vector3Subtract(scelto.box.max, o);

    /* Le mesh del pezzo restano in giro: servono al lotto adesso e al ripiego
     * non instanziato a ogni fotogramma. */
    w->partIdx[i] = (int *)MemAlloc((unsigned int)(scelto.count * (int)sizeof(int)));
    if (w->partIdx[i] == NULL) { MemFree(idx); MemFree(gr); return false; }
    for (int k = 0; k < scelto.count; k++) w->partIdx[i][k] = idx[scelto.first + k];
    w->partIdxN[i] = scelto.count;

    /* I numeri della taglia sono del MASTIO, non di un pezzo indicizzato
     * qualunque: keepScale, keepHalf e keepHigh hanno un solo destinatario, e
     * il secondo pezzo indicizzato - la cripta della domanda E - vorra' i
     * propri. Scriverli qui senza guardia vorrebbe dire che il secondo
     * sovrascrive in silenzio la torre. */
    w->partScale[i] = MeshGroupScale(&scelto, BUILD_FILES[i].voluto,
                                     BUILD_FILES[i].perAltezza);

    /* keepHalf e keepHigh restano del solo mastio: sono la taglia della TORRE,
     * e li leggono la spinta del giocatore e il taglio della camera. */
    if (i == BUILD_KEEP) {
        float lx = (scelto.box.max.x - scelto.box.min.x) * w->partScale[i];
        float lz = (scelto.box.max.z - scelto.box.min.z) * w->partScale[i];
        w->keepHalf = 0.5f * ((lx > lz) ? lx : lz);
        w->keepHigh = (scelto.box.max.y - scelto.box.min.y) * w->partScale[i];
    }

    InstModelCreateSubset(&w->partBatch[i], *m, w->partIdx[i], w->partIdxN[i]);

    MemFree(idx); MemFree(gr);
    return true;
}

/* La scala di un pezzo che e' un FILE INTERO e dichiara una taglia in metri.
 * I pezzi dei kit non la dichiarano - stanno sulla griglia di BUILD_CELL e
 * hanno 'voluto' a zero - e per loro la scala resta 1. */
static float ScalaPezzoIntero(const Model *m, int i)
{
    if (BUILD_FILES[i].voluto <= 0.0f) return 1.0f;
    MeshGroup g = { 0, m->meshCount, GetModelBoundingBox(*m) };
    return MeshGroupScale(&g, BUILD_FILES[i].voluto, BUILD_FILES[i].perAltezza);
}

/* Tutti o nessuno: mezza casa e' peggio di una scatola. */
static void LoadBuildParts(World *w)
{
    for (int i = 0; i < BUILD_KIT_COUNT; i++)
        if (!FileExists(BUILD_FILES[i].file)) return;

    for (int i = 0; i < BUILD_KIT_COUNT; i++) {
        w->buildPart[i] = LoadModel(BUILD_FILES[i].file);
        if (w->buildPart[i].meshCount == 0) {
            TraceLog(LOG_WARNING, "WORLD: %s non caricato, edifici procedurali",
                     BUILD_FILES[i].file);
            for (int k = 0; k <= i; k++) UnloadModel(w->buildPart[k]);
            return;
        }
        for (int k = 0; k < w->buildPart[i].materialCount; k++)
            SetTextureFilter(w->buildPart[i].materials[k].maps[MATERIAL_MAP_DIFFUSE].texture,
                             TEXTURE_FILTER_POINT);
        LightApplyToModel(&w->buildPart[i]);

        /* gBuildMat e' a inizializzatori designati: un BUILD_* aggiunto senza
         * la sua riga esce { NULL, 0, 0 }, e passare NULL a %s e' comportamento
         * indefinito - glibc stampa "(null)", altre librerie non promettono
         * niente. Il pezzo resta alla tavolozza del kit e al modo 0, che e'
         * esattamente cio' che succede quando le texture non ci sono. */
        if (gBuildMat[i].nome == NULL && !gBuildMat[i].uvVere) {
            TraceLog(LOG_WARNING, "WORLD: pezzo %d senza riga in gBuildMat, niente proiezione", i);
            InstModelCreate(&w->partBatch[i], w->buildPart[i]);
            continue;
        }

        /* Il materiale proiettato sostituisce la tavolozza del kit. Se i file
         * non ci sono si resta alla tavolozza e al modo 0: il gioco funziona
         * senza assets/, e questo non e' un caso d'errore. */
        char diff[128], nor[128];
        snprintf(diff, sizeof diff, "assets/textures/%s_diff.jpg", gBuildMat[i].nome);
        snprintf(nor,  sizeof nor,  "assets/textures/%s_nor.jpg",  gBuildMat[i].nome);

        /* I mipmap PRIMA del filtro, come per il terreno: SetTextureFilter()
         * di raylib 5.5 guarda texture.mipmaps e, se ne trova uno solo,
         * ripiega su GL_LINEAR con un avviso. Nell'ordine inverso i mipmap si
         * genererebbero e non si userebbero, e una parete lontana sfarfalla. */
        if (FileExists(diff)) {
            Texture2D td = LoadTexture(diff);
            /* Il file c'e' ma puo' essere troncato: allora td.id resta 0.
             * Accendere lo stesso la proiezione sarebbe peggio che spegnerla -
             * InstFlush() non lega le mappe con id nullo, quindi texture0
             * resterebbe quella del lotto precedente e l'edificio prenderebbe
             * la texture di un altro oggetto invece della tavolozza. */
            if (td.id != 0) {
                GenTextureMipmaps(&td);
                SetTextureFilter(td, TEXTURE_FILTER_TRILINEAR);
                SetTextureWrap(td, TEXTURE_WRAP_REPEAT);
                SostituisciMappa(w->buildPart[i].materials, w->buildPart[i].materialCount,
                                 MATERIAL_MAP_DIFFUSE, td);

                if (FileExists(nor)) {
                    Texture2D tn = LoadTexture(nor);
                    if (tn.id != 0) {
                        GenTextureMipmaps(&tn);
                        SetTextureFilter(tn, TEXTURE_FILTER_TRILINEAR);
                        SetTextureWrap(tn, TEXTURE_WRAP_REPEAT);
                        SostituisciMappa(w->buildPart[i].materials, w->buildPart[i].materialCount,
                                         MATERIAL_MAP_NORMAL, tn);
                    } else {
                        TraceLog(LOG_WARNING, "WORLD: %s non caricata, rilievo piatto", nor);
                    }
                }
                w->buildProj[i] = true;
            } else {
                TraceLog(LOG_WARNING, "WORLD: %s non caricata, si resta alla tavolozza", diff);
            }
        }

        InstModelCreate(&w->partBatch[i], w->buildPart[i]);
        if (w->buildProj[i])
            InstModelProjection(&w->partBatch[i], gBuildMat[i].mode, gBuildMat[i].tile);
    }
    for (int i = 0; i < BUILD_KIT_COUNT; i++) {
        w->buildLoaded[i] = true;
        w->buildOwned[i]  = true;
    }
    w->hasBuildParts = true;
    /* Il conto dei pezzi proiettati dice a colpo d'occhio se assets/textures/
     * c'e': senza, si resta alla tavolozza del kit e non e' un errore. */
    int proiettati = 0;
    for (int i = 0; i < BUILD_KIT_COUNT; i++) if (w->buildProj[i]) proiettati++;
    TraceLog(LOG_INFO, "WORLD: %d pezzi per gli edifici modulari, %d con materiale proiettato",
             BUILD_KIT_COUNT, proiettati);

    /* I pezzi facoltativi NON entrano nel "tutti o nessuno" dei pezzi dei kit:
     * quel controllo esiste perche' mezza casa e' peggio di una scatola, mentre
     * una torre Kenney e una cripta senza statua sono cose intere e giuste. Se
     * il file non c'e', o non e' il pezzo atteso, si ripiega e non e' un errore. */
    for (int i = BUILD_KIT_COUNT; i < BUILD_PART_COUNT; i++) {
        if (!CaricaPezzo(w, i)) continue;
        w->buildLoaded[i] = true;
        LightApplyToModel(&w->buildPart[i]);

        if (BUILD_FILES[i].pezzo >= 0) {
            if (!PreparaPezzo(w, i)) {
                TraceLog(LOG_INFO, "WORLD: %s pezzo %d non preparato, si ripiega",
                         BUILD_FILES[i].file, BUILD_FILES[i].pezzo);
                continue;
            }
        } else {
            w->partScale[i] = ScalaPezzoIntero(&w->buildPart[i], i);
            InstModelCreate(&w->partBatch[i], w->buildPart[i]);
        }

        if (i == BUILD_KEEP) w->hasKeep = true;

        TraceLog(LOG_INFO, "WORLD: pezzo %s x%.2f%s", BUILD_FILES[i].file,
                 (double)w->partScale[i],
                 InstModelReady(&w->partBatch[i]) ? ", a lotti" : "");
    }
}

/* Raylib 5.5 tiene gli indici di una mesh in 'unsigned short': oltre 65.535
 * vertici li tronca, e lo dice con un avviso fra le centinaia di righe del
 * log. Il risultato non e' un errore ma un DIFETTO VISIVO - gli indici si
 * avvolgono e nascono triangoli che attraversano l'oggetto da parte a parte -
 * quindi qui si rifiuta il modello e si torna alla primitiva procedurale, che
 * almeno e' giusta. Meglio una sfera onesta di un masso sfregiato.
 *
 * Preso su boulder_01 di Poly Haven: 67.042 vertici, milleseicento oltre il
 * limite, e nessun modo di accorgersene senza guardare l'oggetto da vicino. */
static bool TroppiVertici(const Model *m, const char *file)
{
    for (int i = 0; i < m->meshCount; i++) {
        if (m->meshes[i].vertexCount <= 65535) continue;
        TraceLog(LOG_WARNING,
                 "WORLD: %s ha una mesh da %d vertici: raylib ne indirizza al "
                 "massimo 65535 e la romperebbe. Modello scartato.",
                 file, m->meshes[i].vertexCount);
        return true;
    }
    return false;
}

/* Prima i lotti, poi gli array: i lotti puntano ai VBO delle mesh, e il
 * modello si scarica dopo di loro. */
static void FreePropVariants(PropVariants *pv)
{
    for (int g = 0; g < pv->n; g++) InstModelFree(&pv->batch[g]);
    MemFree(pv->batch);
    MemFree(pv->scala);
    MemFree(pv->meshIdx);
    MemFree(pv->gruppo);
    pv->batch = NULL; pv->scala = NULL; pv->meshIdx = NULL; pv->gruppo = NULL;
    pv->n = 0;
}

static void LoadExtProps(World *w)
{
    for (int t = 0; t < PROP_COUNT; t++) {
        if (gExtProp[t].file == NULL) continue;
        char alt[256];
        const char *file = TrovaModello(gExtProp[t].file, alt, (int)sizeof alt);
        if (file == NULL) continue;

        Model m = LoadModel(file);
        if (m.meshCount == 0) {          /* formato non supportato o file rotto */
            TraceLog(LOG_WARNING, "WORLD: %s non caricato", file);
            UnloadModel(m);
            continue;
        }
        if (TroppiVertici(&m, file)) { UnloadModel(m); continue; }

        /* L'atlante di Kenney e' una tavolozza: ogni materiale campiona una
         * cella di colore pieno larga pochi pixel. Con i mipmap, da lontano le
         * celle vicine si mescolano. Il filtro a punti lo evita - qui non ho
         * visto differenze a occhio, ma e' il campionamento giusto per una
         * tavolozza, e la distanza massima di disegno arriva a 260 m. */
        for (int i = 0; i < m.materialCount; i++)
            SetTextureFilter(m.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture,
                             TEXTURE_FILTER_POINT);

        LightApplyToModel(&m);
        w->extProp[t]    = m;
        w->hasExtProp[t] = true;

        /* Le varianti: mesh che si toccano in XZ sono lo stesso individuo. */
        int nm = m.meshCount;
        BoundingBox *bb = (BoundingBox *)MemAlloc((unsigned int)(nm * (int)sizeof(BoundingBox)));
        PropVariants *pv = &w->propVar[t];
        pv->meshIdx = (int *)MemAlloc((unsigned int)(nm * (int)sizeof(int)));
        pv->gruppo  = (MeshGroup *)MemAlloc((unsigned int)(nm * (int)sizeof(MeshGroup)));
        if (bb == NULL || pv->meshIdx == NULL || pv->gruppo == NULL) {
            MemFree(bb); FreePropVariants(pv); UnloadModel(m);
            w->hasExtProp[t] = false;
            continue;
        }

        for (int i = 0; i < nm; i++) bb[i] = GetMeshBoundingBox(m.meshes[i]);
        int ng = MeshGroupSplit(bb, nm, pv->meshIdx, pv->gruppo, nm);
        MemFree(bb);

        /* Oltre MESHGROUP_MAX mesh il raggruppamento si arrende: si tratta il
         * modello come un individuo solo. Non e' il comportamento di prima
         * delle varianti - i vertici vengono ricentrati comunque, piu' sotto -
         * e' solo un individuo solo anziche' un set. */
        if (ng == 0) {
            for (int i = 0; i < nm; i++) pv->meshIdx[i] = i;
            pv->gruppo[0].first = 0;
            pv->gruppo[0].count = nm;
            pv->gruppo[0].box   = GetModelBoundingBox(m);
            ng = 1;
        }

        pv->batch = (InstModel *)MemAlloc((unsigned int)(ng * (int)sizeof(InstModel)));
        pv->scala = (float *)MemAlloc((unsigned int)(ng * (int)sizeof(float)));
        if (pv->batch == NULL || pv->scala == NULL) {
            FreePropVariants(pv); UnloadModel(m);
            w->hasExtProp[t] = false;
            continue;
        }
        pv->n = ng;

        for (int g = 0; g < ng; g++) {
            /* raylib fonde le trasformazioni dei nodi dentro i vertici, quindi
             * la seconda variante porta cucito l'offset che la mette in fila:
             * si toglie qui, una volta, invece che a ogni fotogramma. */
            Vector3 o = MeshGroupOrigin(&pv->gruppo[g]);
            for (int k = 0; k < pv->gruppo[g].count; k++) {
                Mesh *me = &m.meshes[pv->meshIdx[pv->gruppo[g].first + k]];
                /* MeshGroupRecenter() si difende da vertices nullo, ma
                 * UpdateMeshBuffer() no: deferenzia anche vboId. Dopo
                 * LoadModel() non capita mai, ma se capitasse la mesh va
                 * saltata qui, non lasciata cadere dentro la funzione che
                 * la scrive sulla scheda. */
                if (me->vertices == NULL) continue;
                MeshGroupRecenter(me->vertices, me->vertexCount, o);
                UpdateMeshBuffer(*me, 0, me->vertices,
                                 me->vertexCount * 3 * (int)sizeof(float), 0);
            }

            /* pv->gruppo[g].box va riportato all'origine appena tolta, o
             * resterebbe in coordinate pre-ricentraggio: descriverebbe dove la
             * variante ERA, non dov'e' adesso. Oggi nessuno lo legge come
             * posizione assoluta (il disegno usa first/count, e
             * MeshGroupScale usa solo differenze, invarianti per traslazione)
             * ma un domani un raggio di culling ricavato da qui direbbe una
             * bugia. */
            pv->gruppo[g].box.min = Vector3Subtract(pv->gruppo[g].box.min, o);
            pv->gruppo[g].box.max = Vector3Subtract(pv->gruppo[g].box.max, o);

            /* Ogni variante alla stessa taglia, partendo dal proprio ingombro. */
            pv->scala[g] = MeshGroupScale(&pv->gruppo[g], gExtProp[t].voluto,
                                          gExtProp[t].perAltezza);

            InstModelCreateSubset(&pv->batch[g], m,
                                  pv->meshIdx + pv->gruppo[g].first,
                                  pv->gruppo[g].count);
        }

        TraceLog(LOG_INFO, "WORLD: modello esterno %s (%d mesh, %d variant%s, "
                 "x%.2f -> %.1f m)%s",
                 file, nm, ng, (ng == 1) ? "e" : "i", (double)pv->scala[0],
                 (double)gExtProp[t].voluto,
                 InstModelReady(&pv->batch[0]) ? ", a lotti" : "");
    }

    /* Il tumulo vuole un SET. Con un modello a una variante - la cripta del kit
     * rimessa a mano - si torna a disegnare un oggetto solo: quindici copie
     * della stessa lastra in cerchio non sono un tumulo. */
    w->hasTumulo = (w->propVar[PROP_CRYPT].n >= 2);
    TraceLog(LOG_INFO, "WORLD: cripta %s (%d variant%s)",
             w->hasTumulo ? "a tumulo" : "a oggetto singolo",
             w->propVar[PROP_CRYPT].n, (w->propVar[PROP_CRYPT].n == 1) ? "e" : "i");
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

            /* Niente luce cotta nei vertici quando c'e' il sole vero: si
             * sommerebbe a quella dello shader e le colline sarebbero scure
             * due volte. Senza shader resta la vecchia illuminazione fissa. */
            float lit = 1.0f;
            if (!LightReady()) {
                float diff = Vector3DotProduct(nrm, sun);
                if (diff < 0.0f) diff = 0.0f;
                lit = 0.42f + 0.58f * diff;
            }

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

    /* Il sole illumina anche le primitive e il terreno: senza questa riga
     * resterebbero piatti mentre il resto della scena e' illuminato. */
    LightApplyToMaterial(&w->terrainMat);
    LightApplyToModel(&w->mCyl);
    LightApplyToModel(&w->mCone);
    LightApplyToModel(&w->mSphere);
    LightApplyToModel(&w->mCube);

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
    for (int t = 0; t < PROP_COUNT; t++) {
        /* Prima il lotto, poi il modello: il lotto punta ai VBO della mesh. */
        FreePropVariants(&w->propVar[t]);
        if (w->hasExtProp[t]) { UnloadModel(w->extProp[t]); w->hasExtProp[t] = false; }
    }
    /* Non piu' condizionato a hasBuildParts: il mastio si carica anche quando i
     * pezzi dei kit non ci sono tutti, e va scaricato lo stesso. */
    for (int i = 0; i < BUILD_PART_COUNT; i++) {
        /* Prima i lotti, poi il modello: i lotti puntano ai suoi VBO. */
        InstModelFree(&w->partBatch[i]);
        MemFree(w->partIdx[i]);
        w->partIdx[i]  = NULL;
        w->partIdxN[i] = 0;
        /* Chi riusa il file di un altro non lo scarica: la seconda
         * UnloadModel() colpirebbe VBO gia' liberati. */
        if (w->buildLoaded[i] && w->buildOwned[i]) UnloadModel(w->buildPart[i]);
        w->buildLoaded[i] = false;
        w->buildOwned[i]  = false;
    }
    w->hasBuildParts = false;
    w->hasKeep       = false;

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

    /* Se il pezzo ha un lotto si accoda e basta: il disegno avviene alla fine
     * del passaggio, tutte le istanze insieme. Altrimenti si disegna qui, come
     * si e' sempre fatto. */
    if (InstModelReady(&w->partBatch[part]))
        InstModelAdd(&w->partBatch[part], p, rotDeg + localRot, scale);
    else {
        /* Ripiego: si arriva qui quando manca assets/shaders/ o un lotto non
         * si e' creato. L'uniform e' per lotto, e qui di lotto non ce n'e'. */
        if (w->buildProj[part])
            LightSetProjection(gBuildMat[part].mode, gBuildMat[part].tile);

        if (w->partIdx[part] != NULL) {
            /* Un pezzo dentro un file di venti: DrawModelEx li disegnerebbe
             * TUTTI, in un mucchio attorno alla torre. Una mesh per volta, e
             * solo le sue - la stessa strada che DrawProp fa per le varianti. */
            Model *mo = &w->buildPart[part];
            Matrix mt = MatrixMultiply(
                            MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z),
                                           MatrixRotateY((rotDeg + localRot) * DEG2RAD)),
                            MatrixTranslate(p.x, p.y, p.z));
            for (int j = 0; j < w->partIdxN[part]; j++) {
                int mi  = w->partIdx[part][j];
                int mat = (mo->meshMaterial != NULL) ? mo->meshMaterial[mi] : 0;
                if (mat < 0 || mat >= mo->materialCount) mat = 0;

                /* Copia superficiale, e l'assegnazione e' ASSOLUTA: mm.maps
                 * resta l'array del modello, quindi questa riga ci scrive
                 * davvero, ma riparte ogni fotogramma dallo stesso valore e non
                 * accumula. Stessa convenzione di InstTint(). Se diventasse una
                 * moltiplicazione, il materiale si scurirebbe a ogni frame. */
                Material mm = mo->materials[mat];
                mm.maps[MATERIAL_MAP_DIFFUSE].color = tint;
                DrawMesh(mo->meshes[mi], mm, mt);
            }
        } else {
            DrawModelEx(w->buildPart[part], p, (Vector3){ 0.0f, 1.0f, 0.0f },
                        rotDeg + localRot, scale, tint);
        }

        if (w->buildProj[part]) LightSetProjection(0, 1.0f);
    }
}

/* Casa: 3x2 celle, muri sul perimetro, una porta al centro della facciata,
 * finestre altrove, e un tetto a due falde allungato sulla profondita'.
 * I pezzi del kit stanno sul bordo +X della loro cella: per portarli sugli
 * altri lati si ruota di 90 gradi alla volta. La porta e' un arco aperto, non
 * un battente: ci si passa davvero, e da fuori si vede che si puo' entrare. */
/* --- Forma di una casa ---------------------------------------------------
 * Due tagli, decisi dalla posizione: la maggior parte sono case basse 3x2, una
 * su tre e' un edificio 4x3 con il primo piano e una scala per salirci.
 *
 * La forma e' una FUNZIONE della posizione, non un dato: il mondo cotto resta
 * quello di prima, e disegno e collisione la calcolano allo stesso modo. Se
 * divergessero si camminerebbe su un piano che non c'e'. */
typedef struct { int nx, nz, floors, stairX, stairZ; } HouseShape;

static HouseShape HouseShapeOf(const Prop *p)
{
    HouseShape s = { 3, 2, 1, 0, 0 };
    if (FmHash01((unsigned int)(p->pos.x * 4.0f), (int)(p->pos.z * 4.0f), 77) > 0.62f) {
        s.nx = 4; s.nz = 3; s.floors = 2;
        s.stairX = s.nx - 2;    /* la scala sale lungo +X dentro la sua cella: */
        s.stairZ = s.nz - 1;    /* in cima si arriva sull'ultima colonna       */
    }
    return s;
}

/* Semipianta in celle. I pannelli stanno a 0,45 celle dal centro della loro
 * cella, quindi il muro cade a meta' pianta meno 0,05. */
static float HouseHalfX(const HouseShape *s) { return s->nx * 0.5f - 0.05f; }
static float HouseHalfZ(const HouseShape *s) { return s->nz * 0.5f - 0.05f; }

/* Quota della rampa dentro la cella della scala, in celle: sale da 0 a 1
 * lungo +X, e fuori dalla cella resta agganciata agli estremi. La usano sia
 * chi ci cammina sopra sia chi ci sbatte contro: un conto solo, cosi' la
 * superficie calpestabile e il volume solido non possono divergere. */
static float StairTop(const HouseShape *s, float lx)
{
    float edge = (float)s->stairX - s->nx * 0.5f;   /* bordo basso, in celle */
    return FmClamp(lx - edge, 0.0f, 1.0f);
}

static void DrawHouse(World *w, const Prop *p, Vector3 pos, float rotDeg,
                     float s, Color tint)
{
    HouseShape sh = HouseShapeOf(p);
    float cell = BUILD_CELL * s;
    Vector3 sc = { cell, cell, cell };

    for (int f = 0; f < sh.floors; f++) {
        float y = (float)f;                    /* in celle: un piano e' alto una */

        for (int ix = 0; ix < sh.nx; ix++) {
            float lx = (float)ix - (sh.nx - 1) / 2.0f;
            for (int iz = 0; iz < sh.nz; iz++) {
                float lz = (float)iz - (sh.nz - 1) / 2.0f;
                bool isStair = (sh.floors > 1 && ix == sh.stairX && iz == sh.stairZ);

                /* Il solaio del piano terra e' il pavimento; quello sopra e' il
                 * piano su cui si cammina, e sopra la scala manca: e' la
                 * tromba da cui si sale. */
                if (!(f > 0 && isStair))
                    PlacePart(w, BUILD_FLOOR, pos, rotDeg, lx, y, lz, 0.0f, cell, sc, tint);

                if (f == 0 && isStair)
                    PlacePart(w, BUILD_STAIRS, pos, rotDeg, lx, y, lz, 0.0f, cell, sc, tint);

                if (ix == sh.nx - 1)
                    PlacePart(w, BUILD_WALL, pos, rotDeg, lx, y, lz,   0.0f, cell, sc, tint);
                if (ix == 0)
                    PlacePart(w, BUILD_WALL, pos, rotDeg, lx, y, lz, 180.0f, cell, sc, tint);
                if (iz == 0)
                    PlacePart(w, (f == 0 && ix == sh.nx / 2) ? BUILD_DOOR : BUILD_WINDOW,
                              pos, rotDeg, lx, y, lz,  90.0f, cell, sc, tint);
                if (iz == sh.nz - 1)
                    PlacePart(w, BUILD_WINDOW, pos, rotDeg, lx, y, lz, 270.0f, cell, sc, tint);
            }
        }
    }

    /* Il pezzo del tetto e' un segmento a due falde largo una cella: due
     * affiancati farebbero una valle in mezzo, quindi se ne allunga uno solo
     * sulla profondita' e si alza il colmo per non appiattire la pendenza.
     * La falda e' un guscio sottile: da sotto se ne vedrebbe attraverso,
     * quindi per questi pezzi lo scarto delle facce posteriori si spegne -
     * dentro casa serve un soffitto. */
    Vector3 roofSc = { cell, cell * 1.6f, cell * sh.nz };
    /* Lo scarto delle facce posteriori si spegne qui SOLO per il ripiego senza
     * lotti, dove PlacePart disegna davvero. Quando i lotti ci sono, il disegno
     * avviene a fine passaggio e questa coppia non lo tocca: ci pensa
     * PartBatchFlush(), che svuota il tetto per conto suo. Dimenticarlo la'
     * darebbe soffitti trasparenti, e da fuori non si vedrebbe. */
    rlDisableBackfaceCulling();
    for (int ix = 0; ix < sh.nx; ix++)
        PlacePart(w, BUILD_ROOF, pos, rotDeg, (float)ix - (sh.nx - 1) / 2.0f,
                  (float)sh.floors, 0.0f, 0.0f, cell, roofSc, tint);
    rlEnableBackfaceCulling();
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

/* Mastio: un pezzo solo, appoggiato a terra.
 *
 * Non usa la griglia delle celle - BUILD_CELL vale 2,6 m e regge le case, non
 * una torre da 15,84 - quindi la scala e' metrica e arriva dall'ingombro
 * misurato al caricamento. Con lx = ly = lz = 0 la cella non entra nel conto,
 * e si passa 1 per dirlo. */
static void DrawKeep(World *w, Vector3 pos, float rotDeg, float s, Color tint)
{
    float k = w->partScale[BUILD_KEEP] * s;
    PlacePart(w, BUILD_KEEP, pos, rotDeg, 0.0f, 0.0f, 0.0f, 0.0f,
              1.0f, (Vector3){ k, k, k }, tint);
}

/* --- Il tumulo della cripta ----------------------------------------------
 * La cripta non e' un oggetto ma una ricetta: un anello di massi con un varco.
 * Un edificio del genere non esiste come scansione - cercato su 521 modelli del
 * catalogo - quindi si compone, e i pezzi sono i sei massi di rock_moss_set_01.
 *
 * I numeri, e da dove vengono. Il masso normalizzato e' 2,8 m sul lato XZ
 * maggiore; il passo fra due posti e' 2,02 m, cioe' MENO del masso, cosi' si
 * toccano e l'anello si legge come un muro invece che come una fila di sassi.
 *
 * Due posti vuoti e non uno ne' tre: la corda fra i massi che fiancheggiano il
 * varco e' 2R*sin((v+1)*PI/17) meno 2,8 di pietra. Con UNO restano 1,17 m - si
 * passa di striscio e da fuori non si vede che e' un ingresso. Con TRE ne
 * restano 4,61, e non e' piu' un varco ma un lato aperto. Con DUE: 2,99 m. */
#define CRYPT_RAGGIO      5.5f
#define CRYPT_POSTI       17
#define CRYPT_VUOTI       2
#define CRYPT_MASSO       2.8f     /* lato XZ maggiore, normalizzato */
#define CRYPT_MAX_MASSI   CRYPT_POSTI

/* Un masso del tumulo, in coordinate LOCALI rispetto al centro della cripta.
 *
 * La quota non c'e' apposta: la mette chi disegna, prendendola dal terreno
 * sotto quel punto meno 'affonda'. Cosi' l'anello segue il pendio, e la ricetta
 * resta geometria pura - si prova senza mondo caricato e senza GPU. */
typedef struct {
    float dx, dz;      /* scostamento dal centro, in metri */
    int   variante;    /* quale dei massi del set          */
    float scala;       /* sopra la taglia normalizzata     */
    float yaw;         /* gradi attorno a Y                */
    float affonda;     /* quanto sprofonda, in metri       */
} CryptStone;

/* Un valore riproducibile per il masso k di questa cripta. Il sale tiene le
 * decisioni indipendenti fra loro: la variante di un masso non deve correlare
 * con la sua rotazione, o l'anello mostrerebbe uno schema. */
static float CryptHash(const Prop *p, int k, int sale)
{
    return FmHash01((unsigned int)(p->pos.x * 4.0f),
                    (int)(p->pos.z * 4.0f) + k * 101, sale);
}

/* Il raggio del cerchio di collisione di un masso.
 *
 * CIRCOSCRIVE il masso invece di inscriverlo: blocca un po' piu' della pietra
 * vera, ed e' la scelta voluta - meglio sfiorare un bordo invisibile che
 * entrare dentro la roccia. */
static float CryptStoneRadius(const CryptStone *s)
{
    return CRYPT_MASSO * 0.5f * s->scala;
}

/* I massi del tumulo. E' una FUNZIONE della posizione, non un dato: il mondo
 * cotto non cambia, e disegno, spinta del giocatore e taglio della camera
 * arrivano tutti agli stessi massi. Se divergessero si sbatterebbe contro un
 * masso che non c'e'.
 *
 * Torna quanti ne ha scritti. */
static int CryptRing(const Prop *p, CryptStone *out, int max)
{
    if (p == NULL || out == NULL || max <= 0) return 0;

    /* Da quale posto comincia il varco. Dalla posizione, cosi' due cripte non
     * hanno l'ingresso nello stesso punto. */
    float h = CryptHash(p, 0, 151);
    int primoVuoto = (int)(h * (float)CRYPT_POSTI);
    if (primoVuoto >= CRYPT_POSTI) primoVuoto = CRYPT_POSTI - 1;   /* h == 1 */

    int n = 0;
    for (int k = 0; k < CRYPT_POSTI && n < max; k++) {
        /* I posti vuoti sono CRYPT_VUOTI consecutivi: consecutivi e non sparsi,
         * o sarebbero tanti buchi invece di un ingresso. */
        int rel = (k - primoVuoto + CRYPT_POSTI) % CRYPT_POSTI;
        if (rel < CRYPT_VUOTI) continue;

        float ang = (float)k * (2.0f * PI / (float)CRYPT_POSTI);
        CryptStone *s = &out[n++];
        s->dx = cosf(ang) * CRYPT_RAGGIO;
        s->dz = sinf(ang) * CRYPT_RAGGIO;

        /* Sei forme su quindici posti: senza varieta' si vedrebbe la ripetizione
         * a colpo d'occhio. */
        float hv = CryptHash(p, k, 131);
        s->variante = (int)(hv * 6.0f);
        if (s->variante > 5) s->variante = 5;

        s->scala   = 0.85f + CryptHash(p, k, 137) * 0.40f;
        s->yaw     = CryptHash(p, k, 139) * 360.0f;
        s->affonda = CryptHash(p, k, 149) * 0.30f;
    }
    return n;
}

/* Disegna o accoda i massi del tumulo. 'aLotti' dice quale delle due: i lotti
 * quando ci sono, un oggetto per volta nel ripiego.
 *
 * La quota di ogni masso viene dal TERRENO sotto di lui, non dal centro della
 * cripta: quindici massi su undici metri di diametro, su un pendio, appoggiati
 * tutti alla stessa quota sarebbero mezzi sospesi e mezzi sepolti.
 *
 * Torna quanti massi ha messo: zero vuol dire che il chiamante deve ripiegare. */
static int CryptDraw(World *w, const Prop *p, Color tint, bool aLotti)
{
    PropVariants *pv = &w->propVar[PROP_CRYPT];
    if (pv->n == 0) return 0;

    /* Se si accodasse a meta' e poi ci si arrendesse, il chiamante ripiegherebbe
     * e disegnerebbe TUTTI i massi una seconda volta - quelli gia' accodati due
     * volte. Quindi la resa si decide PRIMA di accodare qualunque cosa. */
    if (aLotti)
        for (int v = 0; v < pv->n; v++)
            if (!InstModelReady(&pv->batch[v])) return 0;

    CryptStone anello[CRYPT_MAX_MASSI];
    int n = CryptRing(p, anello, CRYPT_MAX_MASSI);

    int messi = 0;
    for (int i = 0; i < n; i++) {
        CryptStone *s = &anello[i];
        /* La variante viene dalla ricetta, ma il set potrebbe averne meno di
         * sei: si riporta dentro invece di leggere fuori dall'array. */
        int v = s->variante % pv->n;
        float k = p->scale * pv->scala[v] * s->scala;

        Vector3 pos = { p->pos.x + s->dx * p->scale, 0.0f, p->pos.z + s->dz * p->scale };
        pos.y = WorldHeight(w, pos.x, pos.z) - s->affonda;

        if (aLotti) {
            InstModelAdd(&pv->batch[v], pos, s->yaw, (Vector3){ k, k, k });
            messi++;
            continue;
        }

        Matrix mt = MatrixMultiply(
                        MatrixMultiply(MatrixScale(k, k, k),
                                       MatrixRotateY(s->yaw * DEG2RAD)),
                        MatrixTranslate(pos.x, pos.y, pos.z));
        Model *mo = &w->extProp[PROP_CRYPT];
        for (int j = 0; j < pv->gruppo[v].count; j++) {
            int mi  = pv->meshIdx[pv->gruppo[v].first + j];
            int mat = (mo->meshMaterial != NULL) ? mo->meshMaterial[mi] : 0;
            if (mat < 0 || mat >= mo->materialCount) mat = 0;
            Material mm = mo->materials[mat];
            mm.maps[MATERIAL_MAP_DIFFUSE].color = Shade(WHITE, tint);
            DrawMesh(mo->meshes[mi], mm, mt);
        }
        messi++;
    }

    /* La statua sta DI FIANCO al varco, non in mezzo: l'ingresso e' il punto in
     * cui il giocatore corre, e una statua nel mezzo si prende una spallata.
     * Sfalsata di mezzo posto oltre il bordo del varco, e arretrata di 1,6 m
     * dall'anello, girata verso il centro. */
    if (w->buildLoaded[BUILD_STATUE]) {
        float hs = CryptHash(p, 0, 151);
        int primoVuoto = (int)(hs * (float)CRYPT_POSTI);
        if (primoVuoto >= CRYPT_POSTI) primoVuoto = CRYPT_POSTI - 1;

        float passo = 2.0f * PI / (float)CRYPT_POSTI;
        float ang   = ((float)primoVuoto - 0.8f) * passo;
        float rad   = (CRYPT_RAGGIO + 1.6f) * p->scale;
        Vector3 sp  = { p->pos.x + cosf(ang) * rad, 0.0f, p->pos.z + sinf(ang) * rad };
        sp.y = WorldHeight(w, sp.x, sp.z);

        float k = w->partScale[BUILD_STATUE] * p->scale;
        /* Girata verso il centro: l'angolo del raggio, piu' mezzo giro. */
        float yaw = -ang * RAD2DEG + 180.0f;
        PlacePart(w, BUILD_STATUE, sp, yaw, 0.0f, 0.0f, 0.0f, 0.0f,
                  1.0f, (Vector3){ k, k, k }, tint);
    }

    return messi;
}

/* Quale individuo del set tocca a questo prop. E' una FUNZIONE della
 * posizione, non un dato: il mondo cotto non cambia, e disegno, passaggio
 * d'ombra e collisione arrivano tutti allo stesso numero.
 *
 * Il sale 91 la tiene indipendente dalle altre decisioni prese dalla
 * posizione: la variante di un cespuglio non deve correlare con la forma
 * delle case, che usa 77. */
static int PropVariantOf(const Prop *p, int n)
{
    if (n <= 1) return 0;
    float h = FmHash01((unsigned int)(p->pos.x * 4.0f), (int)(p->pos.z * 4.0f), 91);
    int v = (int)(h * (float)n);
    return (v >= n) ? n - 1 : v;      /* h == 1 non deve uscire dall'array */
}

static void DrawProp(World *w, const Prop *p, Color tint, bool lod)
{
    const Vector3 Y = { 0.0f, 1.0f, 0.0f };
    float s = p->scale;
    Vector3 pos = p->pos;

    /* Modello esterno al posto delle primitive, se e' stato scaricato.
     * Shade(WHITE, tint) lascia passare i colori del modello e ci applica solo
     * il ciclo giorno/notte: un tint diverso da WHITE li scurirebbe due volte. */
    if (w->hasExtProp[p->type] && w->propVar[p->type].n > 0) {
        if (p->taken) return;

        /* Il tumulo, quando i lotti non ci sono: una mesh per volta. */
        if (p->type == PROP_CRYPT && w->hasTumulo) {
            CryptDraw(w, p, tint, false);
            return;
        }

        PropVariants *pv = &w->propVar[p->type];
        int v = PropVariantOf(p, pv->n);
        float k = s * pv->scala[v];

        /* Ripiego non instanziato - si arriva qui solo se scene_inst.vs manca
         * o un lotto non si e' creato. La soglia dell'alfa va messa a mano:
         * chi instanzia ce l'ha per lotto, qui no. Senza, il fogliame
         * tornerebbe a quadrati opachi proprio nella modalita' degradata. */
        Model *mo = &w->extProp[p->type];
        float cut = LightAlphaCutFor(mo->materials[0]);
        if (cut > 0.0f) LightSetAlphaCut(cut);

        /* Una mesh per volta, e solo quelle della variante scelta: le altre
         * varianti sono ricentrate sulla stessa origine e si accavallerebbero. */
        Matrix mt = MatrixMultiply(
                        MatrixMultiply(MatrixScale(k, k, k),
                                       MatrixRotateY(p->rot * DEG2RAD)),
                        MatrixTranslate(pos.x, pos.y, pos.z));

        for (int j = 0; j < pv->gruppo[v].count; j++) {
            int mi  = pv->meshIdx[pv->gruppo[v].first + j];
            int mat = (mo->meshMaterial != NULL) ? mo->meshMaterial[mi] : 0;
            if (mat < 0 || mat >= mo->materialCount) mat = 0;

            /* 'mm' e' una copia SUPERFICIALE: Material.maps e' un puntatore,
             * quindi mm.maps resta lo stesso array di mo->materials[mat] e la
             * riga sotto scrive comunque nel materiale del modello. Non fa
             * danno perche' l'assegnazione e' ASSOLUTA (mette Shade(...), non
             * lo moltiplica sul valore precedente): ogni fotogramma riparte
             * dallo stesso WHITE e non si accumula. E' la stessa convenzione
             * di InstTint() in instancing.c, che scrive sull'identico array
             * condiviso (anche b->mat = mat, li', e' superficiale) - le due
             * strade di disegno restano d'accordo apposta. Se questa riga
             * diventasse una moltiplicazione, ACCUMULEREBBE sul materiale del
             * modello a ogni fotogramma: e' la mossa da non fare. */
            Material mm = mo->materials[mat];
            mm.maps[MATERIAL_MAP_DIFFUSE].color = Shade(WHITE, tint);
            DrawMesh(mo->meshes[mi], mm, mt);
        }

        if (cut > 0.0f) LightSetAlphaCut(0.0f);
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
            if (w->hasBuildParts) { DrawHouse(w, p, pos, p->rot, s, Shade(WHITE, tint)); break; }
            Vector3 body = { pos.x, pos.y + 1.7f, pos.z };
            DrawModelEx(w->mCube, body, Y, p->rot, (Vector3){7.0f, 3.4f, 5.5f},
                        Shade((Color){ 176, 156, 126, 255 }, tint));
            Vector3 roof = { pos.x, pos.y + 3.4f, pos.z };
            DrawModelEx(w->mCone, roof, Y, p->rot + 45.0f, (Vector3){5.6f, 2.6f, 5.6f},
                        Shade((Color){ 108, 62, 48, 255 }, tint));
        } break;
        case PROP_TOWER: {
            /* Tre gradini di ripiego: il mastio se c'e', i pezzi del kit se no,
             * il cilindro procedurale se non c'e' nemmeno quello. */
            if (w->hasKeep)       { DrawKeep(w, pos, p->rot, s, Shade(WHITE, tint)); break; }
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

/* Svuota le liste dei lotti dei prop. Si fa per PASSAGGIO e non per
 * fotogramma: il passaggio principale culla a cono, quello d'ombra a raggio,
 * quindi le liste sono diverse. */
static void PropBatchBegin(World *w, Color tint)
{
    /* La tinta del ciclo giorno/notte moltiplica l'albedo, ed e' uguale per
     * tutti: e' del lotto, non dell'istanza. Shade(WHITE, tint) vale tint, ed
     * e' il conto che faceva DrawProp per i modelli esterni. */
    for (int t = 0; t < PROP_COUNT; t++)
        for (int g = 0; g < w->propVar[t].n; g++)
            InstModelBegin(&w->propVar[t].batch[g], tint);
    for (int i = 0; i < BUILD_PART_COUNT; i++) InstModelBegin(&w->partBatch[i], tint);
}

static void PropBatchFlush(World *w)
{
    for (int t = 0; t < PROP_COUNT; t++)
        for (int g = 0; g < w->propVar[t].n; g++)
            InstModelFlush(&w->propVar[t].batch[g]);

    /* I pezzi d'edificio. Il tetto per ultimo e da solo: e' un guscio sottile,
     * e da dentro casa se ne vedrebbe attraverso, quindi va disegnato con lo
     * scarto delle facce posteriori spento. */
    for (int i = 0; i < BUILD_PART_COUNT; i++)
        if (i != BUILD_ROOF) InstModelFlush(&w->partBatch[i]);

    if (InstModelReady(&w->partBatch[BUILD_ROOF])) {
        rlDisableBackfaceCulling();
        InstModelFlush(&w->partBatch[BUILD_ROOF]);
        rlEnableBackfaceCulling();
    }
}

/* Accoda il prop al suo lotto. Torna false se il lotto non c'e' - modello
 * assente, piu' di una mesh, shader instanziato mancante - e allora il
 * chiamante disegna un oggetto per volta come si e' sempre fatto. */
static bool PropBatchAdd(World *w, const Prop *p)
{
    /* Il tumulo non e' un'istanza ma quindici. Sta qui e non in DrawProp perche'
     * da qui passano ENTRAMBI i passaggi - principale e ombra - e scriverlo due
     * volte vorrebbe dire due ricette da tenere d'accordo.
     *
     * WHITE e non la tinta: quella del ciclo giorno/notte e' del LOTTO, la mette
     * gia' PropBatchBegin, e passarla qui la applicherebbe due volte. */
    if (p->type == PROP_CRYPT && w->hasTumulo)
        return CryptDraw(w, p, WHITE, true) > 0;

    PropVariants *pv = &w->propVar[p->type];
    if (pv->n == 0 || p->taken) return false;

    int v = PropVariantOf(p, pv->n);
    if (!InstModelReady(&pv->batch[v])) return false;

    float k = p->scale * pv->scala[v];
    InstModelAdd(&pv->batch[v], p->pos, p->rot, (Vector3){ k, k, k });
    return true;
}

void WorldDrawProps(World *w, Camera3D cam, Color tint)
{
    /* La direzione della camera si normalizzava una volta per prop: con 6000
     * props sono 6000 radici quadrate buttate. Si calcola qui, una volta. */
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float lodD2 = PROP_LOD_DIST * PROP_LOD_DIST;

    PropBatchBegin(w, tint);

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

            if (!PropBatchAdd(w, p)) DrawProp(w, p, tint, d2 > lodD2);
        }
    }

    PropBatchFlush(w);
}

/* Cio' che proietta ombra attorno al giocatore. Non usa il cono visivo: il
 * sole guarda da un'altra parte, e un albero fuori inquadratura puo' benissimo
 * proiettare dentro. */
void WorldDrawShadowCasters(World *w, Vector3 center, float radius)
{
    float r2 = radius * radius;

    /* I lotti vanno svuotati e riempiti anche QUI, e non e' un dettaglio: i
     * pezzi d'edificio passano da PlacePart, che accoda invece di disegnare.
     * Senza questo giro le case accodate nel passaggio d'ombra verrebbero
     * buttate via dall'InstBegin del passaggio principale, che gira dopo - e
     * gli edifici smetterebbero di proiettare ombra. Misurato mentre
     * succedeva: le chiamate del passaggio d'ombra erano crollate da 399 a
     * 126, e sembrava un guadagno.
     *
     * Nel passaggio d'ombra la tinta non serve, si scrive solo profondita'. */
    PropBatchBegin(w, WHITE);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        float cx = (c->cx + 0.5f) * CHUNK_SIZE, cz = (c->cz + 0.5f) * CHUNK_SIZE;
        float dx = cx - center.x, dz = cz - center.z;
        if (dx * dx + dz * dz > (radius + CHUNK_SIZE) * (radius + CHUNK_SIZE)) continue;
        DrawMesh(c->mesh, w->terrainMat, c->xform);
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        for (int k = 0; k < c->propCount; k++) {
            Prop *p = &c->props[k];
            /* Un ciuffo d'erba e un fiore non proiettano niente che si veda, e
             * nel bosco sono la maggioranza dei prop: saltarli dimezza il
             * passaggio senza togliere un'ombra che qualcuno noterebbe. */
            if (p->type == PROP_HERB || p->type == PROP_BUSH) continue;
            float dx = p->pos.x - center.x, dz = p->pos.z - center.z;
            if (dx * dx + dz * dz > r2) continue;

            if (!PropBatchAdd(w, p)) DrawProp(w, p, WHITE, false);
        }
    }

    PropBatchFlush(w);
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

/* --- Collisione degli edifici -------------------------------------------
 * Una casa non e' un cilindro pieno: e' quattro muri con un vano di porta. Le
 * misure vengono dalla stessa ricetta che la disegna (DrawHouse): il pannello
 * del kit sta a 0,45 celle dal centro della sua cella, quindi su una pianta
 * 3x2 i muri cadono a +-1,45 celle in X e +-0,95 in Z.
 *
 * Vale solo quando i pezzi ci sono: senza modelli la casa resta la scatola
 * procedurale, e una scatola piena si aggira, non si attraversa. */
#define HOUSE_HX      1.45f    /* semipianta in celle, asse X */
#define HOUSE_HZ      0.95f    /* semipianta in celle, asse Z */
#define HOUSE_WALL_T  0.15f    /* mezzo spessore del muro, in celle */
#define HOUSE_DOOR_H  0.36f    /* mezza luce della porta, in celle */

/* Porta il punto nel sistema della casa: l'inverso della rotazione che
 * PlacePart() applica ai pezzi. */
static void ToHouseLocal(const Prop *p, float wx, float wz, float *lx, float *lz)
{
    float a = p->rot * DEG2RAD, c = cosf(a), s = sinf(a);
    float dx = wx - p->pos.x, dz = wz - p->pos.z;
    *lx = dx * c - dz * s;
    *lz = dx * s + dz * c;
}

/* Spinge fuori da un rettangolo allineato agli assi, in coordinate locali:
 * si esce dal lato in cui si e' entrati meno. */
static void PushOutRect(float *lx, float *lz, float radius,
                        float cx, float cz, float hx, float hz)
{
    float px = hx + radius - fabsf(*lx - cx);
    float pz = hz + radius - fabsf(*lz - cz);
    if (px <= 0.0f || pz <= 0.0f) return;          /* fuori dal rettangolo */
    if (px < pz) *lx += (*lx < cx) ? -px : px;
    else         *lz += (*lz < cz) ? -pz : pz;
}

static void ResolveHouse(const Prop *p, Vector3 *pos, float radius)
{
    HouseShape sh = HouseShapeOf(p);
    float cell = BUILD_CELL * p->scale;
    float hx = HouseHalfX(&sh) * cell, hz = HouseHalfZ(&sh) * cell;
    float t  = HOUSE_WALL_T * cell, dh = HOUSE_DOOR_H * cell;

    float lx, lz;
    ToHouseLocal(p, pos->x, pos->z, &lx, &lz);

    /* Fuori dall'ingombro con un margine: niente da fare. */
    if (fabsf(lx) > hx + t + radius || fabsf(lz) > hz + t + radius) return;

    float lx0 = lx, lz0 = lz;

    PushOutRect(&lx, &lz, radius,  hx, 0.0f, t, hz + t);   /* muro est   */
    PushOutRect(&lx, &lz, radius, -hx, 0.0f, t, hz + t);   /* muro ovest */
    PushOutRect(&lx, &lz, radius, 0.0f,  hz, hx, t);       /* muro nord  */

    /* La porta e' solo al piano terra: al primo piano la facciata e' chiusa,
     * altrimenti si uscirebbe nel vuoto dal buco della porta di sotto. */
    if (pos->y > p->pos.y + cell * 0.6f) {
        PushOutRect(&lx, &lz, radius, 0.0f, -hz, hx, t);
    } else {
        /* Il vano sta dove DrawHouse mette l'arco: al centro della cella
         * nx/2, che con pianta pari non e' il centro della facciata. Il primo
         * tentativo lo dava per centrato e il giocatore restava fuori,
         * a sbattere contro il muro accanto alla porta. */
        float doorCx = ((float)(sh.nx / 2) - (sh.nx - 1) / 2.0f) * cell;
        float left  = doorCx - dh, right = doorCx + dh;
        PushOutRect(&lx, &lz, radius, (right + hx) * 0.5f, -hz, (hx - right) * 0.5f, t);
        PushOutRect(&lx, &lz, radius, (left - hx) * 0.5f, -hz, (left + hx) * 0.5f, t);
    }

    /* La scala e' un volume, non solo una superficie su cui posare i piedi.
     * Finche' esisteva solo come quota calpestabile la si attraversava: da
     * sopra e di fianco la rampa e' piu' alta di un gradino, WorldSupportHeight
     * la scartava, e si passava dentro al modello. Qui la cella della scala
     * respinge come un muro, ma solo dove la rampa sta piu' in alto di un
     * gradino sopra i piedi: la parte bassa resta aperta, ed e' da li' che si
     * sale. Chi e' gia' sulla rampa ha i piedi alla sua quota e non viene
     * toccato, e dal piano di sopra la tromba resta libera per scendere. */
    if (sh.floors > 1) {
        float top = StairTop(&sh, lx / cell) * cell;
        if (top > (pos->y - p->pos.y) + STEP_UP_REACH) {
            float scx = ((float)sh.stairX - (sh.nx - 1) / 2.0f) * cell;
            float scz = ((float)sh.stairZ - (sh.nz - 1) / 2.0f) * cell;
            PushOutRect(&lx, &lz, radius, scx, scz, 0.5f * cell, 0.5f * cell);
        }
    }

    if (lx == lx0 && lz == lz0) return;

    /* Rimette lo spostamento nel sistema del mondo. */
    float a = p->rot * DEG2RAD, c = cosf(a), s = sinf(a);
    float dx = lx - lx0, dz = lz - lz0;
    pos->x += dx * c + dz * s;
    pos->z += -dx * s + dz * c;
}

/* --- La camera contro gli edifici ---------------------------------------
 * Il giocatore deve restare visibile: se fra lui e la camera si mette un muro,
 * la camera si avvicina invece di guardare l'intonaco. E' la soluzione
 * abituale in terza persona - la trasparenza dell'edificio richiederebbe di
 * ordinare le facce per profondita' e da dentro si vedrebbe peggio.
 *
 * Le scatole sono due, e quale si usa dipende da dove sta il giocatore:
 *   - fuori: la camera non deve ENTRARE nell'ingombro esterno;
 *   - dentro: la camera non deve USCIRE dal vano interno, soffitto compreso.
 * Cosi' lo stesso conto risolve i due casi che si vedono giocando: uscire di
 * casa con la camera rimasta dietro il muro, e guardare in basso da dentro con
 * la camera che sale oltre il soffitto. */

/* Intersezione raggio/scatola allineata agli assi, metodo delle lastre.
 * 'enter' chiede il primo ingresso, altrimenti la prima uscita. */
static bool RayBox(Vector3 o, Vector3 d, Vector3 bmin, Vector3 bmax,
                   bool enter, float maxT, float *hit)
{
    float t0 = 0.0f, t1 = maxT;
    const float *po = &o.x, *pd = &d.x, *pmin = &bmin.x, *pmax = &bmax.x;

    for (int a = 0; a < 3; a++) {
        if (fabsf(pd[a]) < 1e-6f) {
            if (po[a] < pmin[a] || po[a] > pmax[a]) return false;  /* parallelo e fuori */
            continue;
        }
        float inv = 1.0f / pd[a];
        float ta = (pmin[a] - po[a]) * inv;
        float tb = (pmax[a] - po[a]) * inv;
        if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return false;
    }
    *hit = enter ? t0 : t1;
    return true;
}

/* Porta un vettore (non un punto) nel sistema della casa. */
static Vector3 DirToHouseLocal(const Prop *p, Vector3 v)
{
    float a = p->rot * DEG2RAD, c = cosf(a), s = sinf(a);
    return (Vector3){ v.x * c - v.z * s, v.y, v.x * s + v.z * c };
}

/* Raggio contro cilindro verticale: serve ai tronchi, che non sono scatole.
 * Si risolve in pianta e poi si controlla che il punto colpito stia
 * nell'altezza del tronco. */
static bool RayTrunk(Vector3 o, Vector3 d, Vector3 c, float r, float h,
                     float maxT, float *hit)
{
    float ox = o.x - c.x, oz = o.z - c.z;
    float a = d.x * d.x + d.z * d.z;
    if (a < 1e-6f) return false;
    float b = 2.0f * (ox * d.x + oz * d.z);
    float cc = ox * ox + oz * oz - r * r;
    float disc = b * b - 4.0f * a * cc;
    if (disc < 0.0f) return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > maxT) return false;
    float y = o.y + d.y * t;
    if (y < c.y || y > c.y + h) return false;
    *hit = t;
    return true;
}

/* La taglia della torre, in metri. Un numero, DUE USI: la spinta del giocatore
 * e il taglio della camera leggono questo, e non possono divergere.
 *
 * Il raggio della spinta e' cotto nel mondo - worldgen emette 3,0 m - ma il
 * mastio e' largo 15,84: con il raggio cotto si camminerebbe dentro il muro, e
 * ricuocere non basterebbe perche' i mondi gia' salvati resterebbero a 3,0.
 * Quando il mastio c'e', quindi, il dato cotto si scavalca. Non e' una novita':
 * la casa fa gia' esattamente questo, salta il cerchio e va sui muri, perche'
 * la sua forma la decide la ricetta e non il dato.
 *
 * Valgono per il MASTIO, e i due chiamanti le invocano dentro 'if (hasKeep)':
 * senza mastio non cambia niente, e i numeri di prima - 3,0 cotto per la
 * spinta, 1,6 e 12,0 per la scatola della camera - restano dove sono sempre
 * stati. Non c'e' un ramo di ripiego qui dentro apposta: sarebbe un valore che
 * non legge nessuno. */
static float TowerHalf(const World *w, const Prop *p)
{
    return w->keepHalf * p->scale;
}

static float TowerHigh(const World *w, const Prop *p)
{
    return w->keepHigh * p->scale;
}

float WorldCameraClip(const World *w, Vector3 eye, Vector3 dir, float maxDist)
{
    float best = maxDist;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        const Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        float cxm = (c->cx + 0.5f) * CHUNK_SIZE, czm = (c->cz + 0.5f) * CHUNK_SIZE;
        if (fabsf(eye.x - cxm) > CHUNK_SIZE || fabsf(eye.z - czm) > CHUNK_SIZE) continue;

        for (int k = 0; k < c->propCount; k++) {
            const Prop *p = &c->props[k];

            /* Solo cio' che sta a portata del braccio della camera. */
            float dx = p->pos.x - eye.x, dz = p->pos.z - eye.z;
            if (dx * dx + dz * dz > (maxDist + 14.0f) * (maxDist + 14.0f)) continue;

            float hitT;

            /* Tronchi: un albero fra la camera e il giocatore lo nasconde
             * quanto un muro. Solo il fusto, non la chioma: attraversare le
             * foglie non da' fastidio, e fermarsi a ogni ramo darebbe una
             * camera nervosa. */
            if (p->type == PROP_TREE || p->type == PROP_PINE) {
                float sc = p->scale;
                if (RayTrunk(eye, dir, p->pos, 0.30f * sc, 3.0f * sc, best, &hitT)
                    && hitT < best) best = hitT;
                continue;
            }

            /* La torre tonda e' un CILINDRO, non una scatola: una scatola
             * attorno a un tondo occluderebbe quattro angoli vuoti. Il cilindro
             * c'e' gia' - RayTrunk, scritto per i fusti degli alberi.
             *
             * Limite dichiarato: RayTrunk torna solo l'INGRESSO, quindi con
             * l'occhio dentro non taglia. Su un volume pieno non ci si arriva. */
            if (p->type == PROP_TOWER && w->hasKeep) {
                if (RayTrunk(eye, dir, p->pos, TowerHalf(w, p), TowerHigh(w, p),
                             best, &hitT) && hitT < best) best = hitT;
                continue;
            }

            /* Quindici massi non sono una scatola: un cilindro per masso, con
             * lo stesso RayTrunk dei fusti degli alberi. */
            if (p->type == PROP_CRYPT && w->hasTumulo) {
                CryptStone anello[CRYPT_MAX_MASSI];
                int nm = CryptRing(p, anello, CRYPT_MAX_MASSI);
                for (int m = 0; m < nm; m++) {
                    Vector3 sc = { p->pos.x + anello[m].dx * p->scale, p->pos.y,
                                   p->pos.z + anello[m].dz * p->scale };
                    float rr = CryptStoneRadius(&anello[m]) * p->scale;
                    /* Alto quanto il masso piu' alto: una camera che passa sopra
                     * un masso basso non da' fastidio, una che entra in uno alto
                     * si'. */
                    if (RayTrunk(eye, dir, sc, rr, 2.4f * p->scale, best, &hitT)
                        && hitT < best) best = hitT;
                }
                continue;
            }

            /* Torre del kit e cripta: scatole piene, non ci si entra. */
            if (p->type == PROP_TOWER || p->type == PROP_CRYPT) {
                float halfXZ = (p->type == PROP_TOWER) ? 1.6f : 6.0f;
                float high   = (p->type == PROP_TOWER) ? 12.0f : 5.5f;
                Vector3 bmin = { p->pos.x - halfXZ, p->pos.y, p->pos.z - halfXZ };
                Vector3 bmax = { p->pos.x + halfXZ, p->pos.y + high, p->pos.z + halfXZ };
                if (RayBox(eye, dir, bmin, bmax, true, best, &hitT) && hitT < best)
                    best = hitT;
                continue;
            }

            if (p->type != PROP_HOUSE || !w->hasBuildParts) continue;

            HouseShape sh = HouseShapeOf(p);
            float cell = BUILD_CELL * p->scale;
            float t    = HOUSE_WALL_T * cell;
            float m    = CAM_BUILD_MARGIN;
            float hx   = HouseHalfX(&sh) * cell, hz = HouseHalfZ(&sh) * cell;

            float lx, lz;
            ToHouseLocal(p, eye.x, eye.z, &lx, &lz);
            Vector3 o = { lx, eye.y - p->pos.y, lz };
            Vector3 d = DirToHouseLocal(p, dir);

            /* A quale piano si trova l'occhio: il vano in cui la camera deve
             * restare e' quello, non tutto l'edificio. Senza questo conto, su
             * un primo piano la camera finiva dentro il solaio. */
            int storey = (int)floorf(o.y / cell);
            if (storey < 0) storey = 0;
            if (storey > sh.floors - 1) storey = sh.floors - 1;
            float y0 = (float)storey * cell;

            bool inside = fabsf(lx) < hx - t && fabsf(lz) < hz - t &&
                          o.y > 0.0f && o.y < cell * (float)sh.floors;

            Vector3 bmin, bmax;
            if (inside) {          /* resta nel vano del piano: pareti e solai */
                bmin = (Vector3){ -(hx - t) + m, y0 + 0.05f + m, -(hz - t) + m };
                bmax = (Vector3){  (hx - t) - m, y0 + cell - m,   (hz - t) - m };
            } else {               /* non entrare nell'ingombro, tetto compreso */
                bmin = (Vector3){ -(hx + t) - m, 0.0f, -(hz + t) - m };
                bmax = (Vector3){  (hx + t) + m, cell * ((float)sh.floors + 0.95f) + m,
                                   (hz + t) + m };
            }

            if (RayBox(o, d, bmin, bmax, !inside, best, &hitT) && hitT < best)
                best = hitT > 0.0f ? hitT : 0.0f;
        }
    }
    return best;
}

/* --- Su cosa si posano i piedi -------------------------------------------
 * Il terreno non e' piu' l'unica superficie: negli edifici alti c'e' il solaio
 * del primo piano e la rampa della scala. Qui si cerca la piu' alta che stia
 * sotto ai piedi, con un margine per salire un gradino: cosi' salendo la scala
 * ci si alza, e stando al piano terra il solaio di sopra non "risucchia" in
 * alto perche' e' troppo lontano.
 *
 * Le misure vengono dalla stessa HouseShape che disegna l'edificio: se il
 * conto qui e il conto la' divergessero, si camminerebbe sul vuoto. */
/* Quanti piani ha questa casa: serve al gioco per raccontarlo e alle prove. */
int WorldHouseFloors(const Prop *p)
{
    HouseShape s = HouseShapeOf(p);
    return s.floors;
}

float WorldSupportHeight(const World *w, Vector3 pos, float reach)
{
    if (!w->hasBuildParts) return -1e9f;

    float best = -1e9f;
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        const Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        float cxm = (c->cx + 0.5f) * CHUNK_SIZE, czm = (c->cz + 0.5f) * CHUNK_SIZE;
        if (fabsf(pos.x - cxm) > CHUNK_SIZE || fabsf(pos.z - czm) > CHUNK_SIZE) continue;

        for (int k = 0; k < c->propCount; k++) {
            const Prop *p = &c->props[k];
            if (p->type != PROP_HOUSE) continue;

            HouseShape sh = HouseShapeOf(p);
            if (sh.floors < 2) continue;          /* le case basse non hanno solai */

            float cell = BUILD_CELL * p->scale;
            float lx, lz;
            ToHouseLocal(p, pos.x, pos.z, &lx, &lz);
            lx /= cell; lz /= cell;               /* da metri a celle */

            if (fabsf(lx) > HouseHalfX(&sh) - HOUSE_WALL_T) continue;
            if (fabsf(lz) > HouseHalfZ(&sh) - HOUSE_WALL_T) continue;

            int ix = (int)floorf(lx + sh.nx * 0.5f);
            int iz = (int)floorf(lz + sh.nz * 0.5f);

            float surf;
            if (ix == sh.stairX && iz == sh.stairZ) {
                /* La rampa sale lungo +X dentro la sua cella: l'altezza e' la
                 * frazione di cella percorsa. */
                surf = p->pos.y + StairTop(&sh, lx) * cell;
            } else {
                surf = p->pos.y + cell;           /* solaio del primo piano */
            }

            if (surf <= pos.y + reach && surf > best) best = surf;
        }
    }
    return best;
}

/* Serve alla camera: dentro casa non puo' restare arretrata di sei metri. */
bool WorldInsideBuilding(const World *w, Vector3 pos)
{
    if (!w->hasBuildParts) return false;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        const Chunk *c = &w->chunks[i];
        if (!c->active) continue;
        /* le case stanno in pochi chunk: si saltano gli altri */
        float cxm = (c->cx + 0.5f) * CHUNK_SIZE, czm = (c->cz + 0.5f) * CHUNK_SIZE;
        if (fabsf(pos.x - cxm) > CHUNK_SIZE || fabsf(pos.z - czm) > CHUNK_SIZE) continue;
        for (int k = 0; k < c->propCount; k++) {
            const Prop *p = &c->props[k];
            if (p->type != PROP_HOUSE) continue;
            HouseShape sh = HouseShapeOf(p);
            float cell = BUILD_CELL * p->scale;
            float lx, lz;
            ToHouseLocal(p, pos.x, pos.z, &lx, &lz);
            if (fabsf(lx) < (HouseHalfX(&sh) - HOUSE_WALL_T) * cell &&
                fabsf(lz) < (HouseHalfZ(&sh) - HOUSE_WALL_T) * cell) return true;
        }
    }
    return false;
}

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

            /* La casa, quando e' fatta di pezzi, si puo' attraversare dalla
             * porta: la collisione e' sui muri, non su un cerchio. */
            if (p->type == PROP_HOUSE && w->hasBuildParts) {
                ResolveHouse(p, pos, radius);
                continue;
            }

            /* La torre, quando e' il mastio, e' larga cinque volte il raggio
             * cotto nel mondo: lo si scavalca, con la stessa taglia che usa la
             * camera. */
            if (p->type == PROP_TOWER && w->hasKeep) {
                float tdx = pos->x - p->pos.x, tdz = pos->z - p->pos.z;
                float td2 = tdx * tdx + tdz * tdz;
                float trr = TowerHalf(w, p) + radius;
                if (td2 < trr * trr) {
                    if (td2 > 0.0001f) {
                        float td = sqrtf(td2);
                        float tpush = (trr - td) / td;
                        pos->x += tdx * tpush;
                        pos->z += tdz * tpush;
                    } else {
                        /* Sul centro esatto non c'e' una direzione in cui
                         * spingere, e il conto generico si arrende. Su un prop
                         * da 3 m era un bersaglio stretto; il mastio e' largo
                         * 15,84 e sta al centro del villaggio, dove il gioco
                         * mette gente e cose. Si sceglie un asse: uscire da una
                         * parte qualunque e' l'unica cosa migliore di restare
                         * dentro la pietra. Misurato: senza questo, chi finisce
                         * sul centro esatto ci resta. */
                        pos->x += trr;
                    }
                }
                continue;
            }

            /* Il tumulo non e' un cerchio ma quindici: il raggio 5,0 cotto nel
             * mondo lo si scavalca, come per il mastio, e per la stessa ragione
             * - ricuocere non aiuterebbe i mondi gia' salvati.
             *
             * Il centro resta LIBERO, ed e' voluto: il boss nasce esattamente
             * li', e a distanza zero la spinta non avrebbe una direzione. */
            if (p->type == PROP_CRYPT && w->hasTumulo) {
                CryptStone anello[CRYPT_MAX_MASSI];
                int nm = CryptRing(p, anello, CRYPT_MAX_MASSI);
                for (int m = 0; m < nm; m++) {
                    float sx = p->pos.x + anello[m].dx * p->scale;
                    float sz = p->pos.z + anello[m].dz * p->scale;
                    float mdx = pos->x - sx, mdz = pos->z - sz;
                    float md2 = mdx * mdx + mdz * mdz;
                    float mrr = CryptStoneRadius(&anello[m]) * p->scale + radius;
                    if (md2 < mrr * mrr) {
                        if (md2 > 0.0001f) {
                            float md = sqrtf(md2);
                            float mpush = (mrr - md) / md;
                            pos->x += mdx * mpush;
                            pos->z += mdz * mpush;
                        } else {
                            /* Sul centro esatto del masso non c'e' una direzione
                             * in cui spingere, e il conto generico si arrende.
                             * Camminando non ci si arriva - si viene fermati
                             * prima - ma e' la seconda volta che questo caso si
                             * presenta dopo il mastio, e costa tre righe.
                             *
                             * Resta aperto sul percorso GENERICO dei prop, dove
                             * lo stesso punto cieco vale per alberi e sassi:
                             * li' non e' stato toccato perche' cambierebbe il
                             * comportamento di 168.000 oggetti per un caso che
                             * si raggiunge solo teletrasportandosi. */
                            pos->x += mrr;
                        }
                    }
                }
                continue;
            }

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
