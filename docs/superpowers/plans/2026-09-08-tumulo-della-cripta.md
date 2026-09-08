# Il tumulo della cripta — piano di implementazione

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** la cripta smette di essere una lastra del kit e diventa un **tumulo** —
un anello di quindici massi muschiati con un varco da cui si entra, una statua a
fianco dell'ingresso, e dentro lo spazio in cui il boss aspetta.

**Architecture:** la cripta è già un modello esterno con la macchina delle
varianti, quindi il caricamento non cambia: basta che `gExtProp` nomini
`rock_moss_set_01` e il motore trova sei varianti da solo. Il lavoro è una
funzione — `CryptRing()` — che dalla posizione del prop produce i quindici massi,
e che leggono tutti e tre: disegno, spinta del giocatore e taglio della camera.

**Tech Stack:** C99, raylib 5.5, glTF di Poly Haven, `make` + `make prove`.

**Spec:** `docs/superpowers/specs/2026-09-08-tumulo-della-cripta-design.md`

## Global Constraints

- **Zero avvisi.** `-Wall -Wextra`, Linux e Windows.
- **Il gioco funziona senza `assets/`.** Ogni asset mancante è un ripiego: senza
  i massi si torna alla cripta procedurale, senza la statua l'anello si disegna
  lo stesso.
- **Il mondo cotto non si tocca.** La cripta resta un prop nella stessa
  posizione, con lo stesso `radius` 5,0 cotto nel file. Nessun ricuocere.
- **L'anello è una funzione della posizione, non un dato.** Disegno, collisione
  e camera devono ricavare gli stessi massi dallo stesso conto, come per
  `HouseShapeOf()`. Se divergono si sbatte contro un masso che non c'è.
- **Tetto dei vertici: 65.535 per primitiva.** Il masso peggiore ne ha 8.538.
- **Le prove si verificano sabotandole.** Un sabotaggio che non fa cadere niente
  vuol dire che la prova va riscritta.
- **Commenti e messaggi di commit in italiano**, che dicono *perché*.
- **Numeri misurati:** `rock_moss_set_01` ha **6 gruppi**, lato XZ maggiore fra
  **2,11 e 3,37 m**, altezze 1,10–1,77, **8.538** vertici nella primitiva
  peggiore, 63.127 triangoli, 1,94 MB. `gothic_statue`: **1,48 × 1,74 × 1,56 m**,
  1 mesh, 23.314 vertici, 3,98 MB. Anello: raggio **5,5**, **17 posti** a 2,02 m,
  **2 vuoti adiacenti**, **15 massi**, varco **2,99 m**, vano **4,10 m**.

---

### Task 1: Scaricare i massi e la statua

**Files:**
- Nessuna modifica al codice: il sottocomando `polyhaven` di
  `tools/fetch_assets.sh` fa già quello che serve.
- Deliverable: `assets/models/crypt.gltf` e `assets/models/statua.gltf`.

**Interfaces:**
- Produces: i due percorsi che i Task 3 e 6 nominano nelle tabelle.

- [ ] **Step 1: scaricare il set di massi**

```bash
./tools/fetch_assets.sh polyhaven rock_moss_set_01 crypt
```

Lascia `assets/models/crypt.gltf` con il suo `.bin` e le texture in
`assets/models/textures/`, e aggiunge la riga in `assets/CREDITS.md`.

Il nome `crypt` non è casuale: `gExtProp` dichiara
`assets/models/crypt.glb`, e `TrovaModello()` prova prima il `.glb` dichiarato e
poi l'altra estensione — quindi trova il `.gltf` appena arrivato.

- [ ] **Step 2: scaricare la statua**

```bash
./tools/fetch_assets.sh polyhaven gothic_statue statua
```

- [ ] **Step 3: verificare cosa è arrivato**

```bash
ls -la assets/models/crypt.gltf assets/models/statua.gltf
grep -E "crypt|statua" assets/CREDITS.md
```

Atteso: `crypt.gltf` ~0,1 MB, `statua.gltf` ~0,1 MB, due righe di crediti con
`Poly Haven` e `CC0`. Le texture stanno in `assets/models/textures/` e i `.bin`
accanto ai `.gltf`, con i nomi che il pacchetto gli dà — non vanno rinominati, o
il `.gltf` non li trova più.

- [ ] **Step 4: commit**

`assets/` non è versionato tranne `CREDITS.md`, che è il registro della
provenienza e va tenuto.

```bash
git add assets/CREDITS.md
git commit -m "I massi del tumulo e la statua, dal catalogo"
```

---

### Task 2: L'anello, come funzione della posizione

**Files:**
- Modify: `src/world.c` — le costanti e `CryptRing()`, subito prima di
  `PropVariantOf()`
- Create: `tools/prove/tumulo.c`
- Modify: `Makefile` — una riga di collegamento nel bersaglio `prove`

**Interfaces:**
- Consumes: `FmHash01(unsigned int seed, int x, int z)` da `src/fmath.h`;
  `Prop` da `src/worldtypes.h`.
- Produces:
  - `typedef struct { float dx, dz; int variante; float scala; float yaw; float affonda; } CryptStone;`
  - `static int CryptRing(const Prop *p, CryptStone *out, int max)` — torna
    quanti massi ha scritto.
  - `static float CryptStoneRadius(const CryptStone *s)` — il raggio del cerchio
    di collisione, in metri.
  - Le costanti `CRYPT_RAGGIO`, `CRYPT_POSTI`, `CRYPT_VUOTI`, `CRYPT_MASSO`,
    `CRYPT_MAX_MASSI`.

- [ ] **Step 1: scrivere la prova che fallisce**

Crea `tools/prove/tumulo.c`:

```c
/* ============================================================================
 * tumulo.c - La cripta non e' un oggetto ma una RICETTA: un anello di massi
 * con un varco. Le posizioni le produce CryptRing() dalla posizione del prop, e
 * le leggono in tre - disegno, spinta del giocatore, taglio della camera.
 *
 * Se i tre calcolassero anelli diversi si sbatterebbe contro un masso che non
 * c'e', o si attraverserebbe pietra che si vede. Questa prova fissa le
 * proprieta' che nessuno dei tre puo' violare da solo.
 *
 * Niente OpenGL: l'anello e' geometria pura, in coordinate locali. La quota la
 * mette chi disegna, prendendola dal terreno.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "prova.h"

/* Il giocatore ha raggio 0,35: per passare gli serve almeno il suo diametro. */
#define PASSAGGIO   0.70f

static Prop CriptaA(float x, float z)
{
    Prop p = { 0 };
    p.pos   = (Vector3){ x, 0.0f, z };
    p.scale = 1.0f;
    p.rot   = 0.0f;
    p.radius = 5.0f;              /* il raggio cotto nel mondo */
    p.type  = PROP_CRYPT;
    return p;
}

/* Distanza libera fra due massi: da centro a centro, meno la pietra dei due. */
static float Luce(const CryptStone *a, const CryptStone *b)
{
    float dx = a->dx - b->dx, dz = a->dz - b->dz;
    return sqrtf(dx * dx + dz * dz) - CryptStoneRadius(a) - CryptStoneRadius(b);
}

int main(void)
{
    CryptStone anello[CRYPT_MAX_MASSI];
    Prop cripta = CriptaA(700.0f, 540.0f);
    int n = CryptRing(&cripta, anello, CRYPT_MAX_MASSI);

    Ok("l'anello ha 17 posti meno i 2 del varco", n == CRYPT_POSTI - CRYPT_VUOTI);

    /* --- Deterministico, e funzione della POSIZIONE --------------------- */
    CryptStone bis[CRYPT_MAX_MASSI];
    int n2 = CryptRing(&cripta, bis, CRYPT_MAX_MASSI);
    int uguali = (n == n2);
    for (int i = 0; i < n && uguali; i++)
        if (anello[i].dx != bis[i].dx || anello[i].dz != bis[i].dz ||
            anello[i].variante != bis[i].variante ||
            anello[i].scala != bis[i].scala || anello[i].yaw != bis[i].yaw ||
            anello[i].affonda != bis[i].affonda) uguali = 0;
    Ok("la stessa cripta da' sempre lo stesso anello", uguali);

    /* Due cripte diverse non devono avere lo stesso ingresso, o il mondo
     * sembrerebbe stampato con lo stampino. */
    CryptStone altra[CRYPT_MAX_MASSI];
    Prop cripta2 = CriptaA(1200.0f, 300.0f);
    int n3 = CryptRing(&cripta2, altra, CRYPT_MAX_MASSI);
    int diverso = (n3 != n);
    for (int i = 0; i < n && i < n3 && !diverso; i++)
        if (anello[i].dx != altra[i].dx || anello[i].variante != altra[i].variante)
            diverso = 1;
    Ok("due posizioni diverse danno anelli diversi", diverso);

    /* --- Il centro e' libero -------------------------------------------- *
     * Vale piu' di tutti gli altri: il boss nasce ESATTAMENTE a cryptPos, e le
     * entita' usano la stessa spinta del giocatore, che a distanza zero non ha
     * una direzione in cui agire. Un masso al centro vuol dire un boss nella
     * roccia, e non lo si scopre finche' non si arriva in fondo alla quest. */
    float piuVicino = 1e9f;
    for (int i = 0; i < n; i++) {
        float d = sqrtf(anello[i].dx * anello[i].dx + anello[i].dz * anello[i].dz);
        float libero = d - CryptStoneRadius(&anello[i]);
        if (libero < piuVicino) piuVicino = libero;
    }
    Ok("nessun masso copre il centro: il boss non nasce nella pietra",
       piuVicino > 0.0f);
    Ok("il vano interno tiene il boss (raggio 0,75) e chi lo combatte",
       piuVicino > 0.75f + 0.35f);

    /* --- Un varco, e uno solo ------------------------------------------- *
     * I massi escono in ordine di posto, quindi i vicini nell'array sono vicini
     * nell'anello, e l'ultimo torna sul primo. */
    int aperture = 0, dovE = -1;
    float varco = 0.0f;
    for (int i = 0; i < n; i++) {
        float l = Luce(&anello[i], &anello[(i + 1) % n]);
        if (l > PASSAGGIO) { aperture++; if (l > varco) { varco = l; dovE = i; } }
    }
    Ok("c'e' esattamente un'apertura attraversabile", aperture == 1);
    Ok("il varco e' largo come una porta doppia", varco > 2.5f && varco < 3.5f);

    /* E l'anello e' CHIUSO altrove: non solo "senza altre aperture larghe", ma
     * senza NESSUNA fessura - i massi devono sovrapporsi. Senza questo controllo
     * un anello tutto buchi passerebbe quello di sopra ogni volta che il buco
     * piu' largo e' uno solo.
     *
     * Si confronta l'indice, non la larghezza: due fessure identiche renderebbero
     * il confronto fra float una moneta. */
    int tocca = 1;
    for (int i = 0; i < n; i++) {
        if (i == dovE) continue;
        if (Luce(&anello[i], &anello[(i + 1) % n]) > 0.0f) tocca = 0;
    }
    Ok("fuori dal varco i massi si sovrappongono", tocca);

    /* --- Le sei forme --------------------------------------------------- *
     * Su una cripta sola il caso puo' non pescarle tutte; su duecento deve. */
    int visto[6] = { 0 };
    int totale = 0;
    for (int k = 0; k < 200; k++) {
        Prop c = CriptaA(100.0f + (float)k * 7.0f, 200.0f + (float)k * 11.0f);
        int m = CryptRing(&c, anello, CRYPT_MAX_MASSI);
        for (int i = 0; i < m; i++) {
            if (anello[i].variante < 0 || anello[i].variante >= 6) { totale = -1; break; }
            visto[anello[i].variante]++;
            totale++;
        }
        if (totale < 0) break;
    }
    Ok("la variante resta dentro l'array", totale > 0);
    int tutte = (totale > 0);
    for (int v = 0; v < 6; v++) if (visto[v] == 0) tutte = 0;
    Ok("su duecento cripte compaiono tutte e sei le forme", tutte);

    int nessunaDomina = (totale > 0);
    for (int v = 0; v < 6; v++) if (visto[v] > totale / 2) nessunaDomina = 0;
    Ok("nessuna forma si prende meta' dei massi", nessunaDomina);

    return ProveEsito();
}
```

- [ ] **Step 2: aggiungere la riga al Makefile**

In `Makefile`, nel bersaglio `prove`, dopo il blocco di `mastio`:

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/tumulo.c \
	      $(SRC_DIR)/fmath.c $(SRC_DIR)/light.c $(SRC_DIR)/instancing.c \
	      $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c \
	      $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/tumulo
```

`meshgroup.c` **non** si collega: `tumulo.c` lo include, come fanno `mastio` e
`varianti`, e collegarlo duplicherebbe i simboli.

- [ ] **Step 3: eseguire e verificare che NON compili**

```bash
make prove
```

Atteso: errore, `CryptStone`, `CryptRing` e `CryptStoneRadius` non dichiarati.

- [ ] **Step 4: scrivere le costanti e la ricetta**

In `src/world.c`, subito prima di `PropVariantOf()`:

```c
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
         * a colpo d'occhio. Il numero di varianti non e' cablato qui - lo passa
         * chi disegna - ma la SCELTA sta qui, perche' anche la collisione deve
         * sapere quale masso e' per conoscerne la taglia. */
        float hv = CryptHash(p, k, 131);
        s->variante = (int)(hv * 6.0f);
        if (s->variante > 5) s->variante = 5;

        s->scala   = 0.85f + CryptHash(p, k, 137) * 0.40f;
        s->yaw     = CryptHash(p, k, 139) * 360.0f;
        s->affonda = CryptHash(p, k, 149) * 0.30f;
    }
    return n;
}
```

- [ ] **Step 5: eseguire le prove**

```bash
make && make prove
```

Atteso: dieci righe `ok` in `build/prove/tumulo`, zero avvisi, e tutte le altre
prove come prima.

- [ ] **Step 6: sabotare, uno per volta, rimettendo a posto ogni volta**

1. **chiudere il varco**: `#define CRYPT_VUOTI 0` — deve cadere "c'e'
   esattamente un'apertura attraversabile";
2. **allargare i posti**: `#define CRYPT_RAGGIO 9.0f` — a raggio 9 il passo
   diventa 3,3 m, piu' del masso, e l'anello si apre dappertutto: deve cadere
   "fuori dal varco i massi si toccano" **e** "c'e' esattamente un'apertura";
3. **un masso al centro**: in `CryptRing()`, mettere `s->dx = 0.0f; s->dz = 0.0f;`
   per `k == 5` — deve cadere "nessun masso copre il centro";
4. **la variante sempre la stessa**: `s->variante = 0;` — deve cadere "su
   duecento cripte compaiono tutte e sei le forme";
5. **l'anello da un contatore invece che dalla posizione**: aggiungere
   `static int giro = 0;` e usare `giro++` al posto di `k * 101` in
   `CryptHash()` — deve cadere "la stessa cripta da' sempre lo stesso anello".
   È il difetto peggiore di tutti, perche' farebbe divergere disegno e
   collisione a ogni fotogramma.

- [ ] **Step 7: commit**

```bash
git add src/world.c tools/prove/tumulo.c Makefile
git commit -m "L'anello del tumulo e' una funzione della posizione"
```

---

### Task 3: La cripta diventa un tumulo

Questo compito cambia quello che si vede. Prima di lui la cripta è la lastra del
kit; dopo, è l'anello di massi.

**Files:**
- Modify: `src/world.c:174` (la riga di `gExtProp`), `LoadExtProps()` in coda,
  `PropBatchAdd()`, e il ramo degli ext prop in `DrawProp()`
- Modify: `src/world.h` — un campo di `World`

**Interfaces:**
- Consumes: `CryptRing()`, `CryptStone`, `CryptStoneRadius()` dal Task 2;
  `WorldHeight(const World *w, float x, float z)`.
- Produces: `w->hasTumulo`, e `static int CryptDraw(World *w, const Prop *p, Color tint, bool aLotti)`.

- [ ] **Step 1: la riga della tabella**

In `src/world.c`, sostituire la riga di `PROP_CRYPT` in `gExtProp`:

```c
    /* La cripta e' un TUMULO: un anello di massi, non un edificio. Il file e'
     * il set di sei massi muschiati, e la taglia e' quella di UN masso - 2,8 m
     * sul lato XZ maggiore, come i sassi sparsi, perche' un masso si misura in
     * larghezza e non in altezza.
     *
     * Per tornare alla cripta del kit non basta rimettere il file: vanno
     * rimessi anche questi due numeri (5,0 e true), o il modello uscirebbe alto
     * meno di tre metri. */
    [PROP_CRYPT]= { "assets/models/crypt.glb",           2.8f, false },
```

- [ ] **Step 2: la guardia sul set**

In `src/world.h`, dopo il campo `hasKeep`:

```c
    /* Il tumulo c'e' solo se l'asset della cripta e' un SET: con un modello a
     * una variante - la cripta del kit rimessa a mano - si disegna un oggetto
     * solo, come si e' sempre fatto. Senza questa guardia si vedrebbero quindici
     * copie della stessa lastra in cerchio. */
    bool   hasTumulo;
```

In `src/world.c`, in coda a `LoadExtProps()`, subito prima della graffa di
chiusura della funzione:

```c
    w->hasTumulo = (w->propVar[PROP_CRYPT].n >= 2);
    TraceLog(LOG_INFO, "WORLD: cripta %s (%d variant%s)",
             w->hasTumulo ? "a tumulo" : "a oggetto singolo",
             w->propVar[PROP_CRYPT].n, (w->propVar[PROP_CRYPT].n == 1) ? "e" : "i");
```

- [ ] **Step 3: la ricetta che disegna**

In `src/world.c`, subito dopo `CryptRing()`:

```c
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
    return messi;
}
```

- [ ] **Step 4: accodare il tumulo invece del prop singolo**

In `PropBatchAdd()`, come primissima cosa dentro la funzione:

```c
    /* Il tumulo non e' un'istanza ma quindici. Sta qui e non in DrawProp perche'
     * da qui passano ENTRAMBI i passaggi - principale e ombra - e scriverlo due
     * volte vorrebbe dire due ricette da tenere d'accordo. */
    if (p->type == PROP_CRYPT && w->hasTumulo)
        return CryptDraw(w, p, WHITE, true) > 0;
```

`WHITE` e non `tint`: la tinta del ciclo giorno/notte è **del lotto**, non
dell'istanza, e la mette già `PropBatchBegin()`. Passarla qui la applicherebbe
due volte.

- [ ] **Step 5: il ripiego non instanziato**

In `DrawProp()`, dentro il ramo `if (w->hasExtProp[p->type] && ...)`, come prima
riga del blocco:

```c
        /* Il tumulo, quando i lotti non ci sono: una mesh per volta. */
        if (p->type == PROP_CRYPT && w->hasTumulo) {
            CryptDraw(w, p, tint, false);
            return;
        }
```

- [ ] **Step 6: compilare e leggere il registro**

```bash
make && make prove
timeout 12 ./frostmark > /tmp/tumulo.log 2>&1
grep -iE "cripta|modello esterno.*crypt" /tmp/tumulo.log
```

Atteso:

```
WORLD: modello esterno assets/models/crypt.gltf (6 mesh, 6 varianti, x0.83 -> 2.8 m), a lotti
WORLD: cripta a tumulo (6 varianti)
```

La scala `x0.83` è quella della **prima** variante, il masso più largo (3,37 m
di lato): 2,8 / 3,37 = 0,83.

- [ ] **Step 7: guardarlo**

La cripta è lontana dal primo villaggio — `cryptPos` sta a 620, 540 dal
villaggio zero — quindi per vederla serve un binario strumentato, come si è
fatto per il mastio: una copia dei sorgenti fuori dal repo, `game.state = GS_PLAY`
forzato, il giocatore portato a `game.world.cryptPos` più 20 m e girato verso il
centro, `TakeScreenshot()` dopo novanta fotogrammi.

Atteso a occhio: un anello di massi diversi fra loro, appoggiati al terreno, con
un'apertura evidente da un lato e lo spazio libero dentro. Se i massi sono tutti
uguali, `variante` non varia; se sono sospesi o sepolti, la quota non viene da
`WorldHeight`.

- [ ] **Step 8: verificare il ripiego**

```bash
mv assets/models/crypt.gltf /tmp/crypt-via
timeout 10 ./frostmark > /tmp/senza.log 2>&1
grep -iE "cripta" /tmp/senza.log
mv /tmp/crypt-via assets/models/crypt.gltf
```

Atteso: `cripta a oggetto singolo (0 varianti)` e nessun avviso — la cripta
procedurale, la scatola con le quattro colonne, come prima degli asset.

- [ ] **Step 9: commit**

```bash
git add src/world.c src/world.h
git commit -m "La cripta e' un anello di massi, non una lastra"
```

---

### Task 4: La collisione segue l'anello

**Files:**
- Modify: `src/world.c` — `WorldCameraClip()` e `WorldResolveCollision()`
- Modify: `tools/prove/tumulo.c`

**Interfaces:**
- Consumes: `CryptRing()`, `CryptStoneRadius()`, `w->hasTumulo`.
- Produces: nessuna funzione nuova — i due chiamanti leggono la stessa ricetta.

- [ ] **Step 1: scrivere la prova che fallisce**

In `tools/prove/tumulo.c`, prima di `return ProveEsito();`:

```c
    /* --- La spinta vera ------------------------------------------------- *
     * WorldResolveCollision gira sui chunk ma non tocca la GPU: un chunk si
     * costruisce a mano, e la prova resta senza contesto grafico. */
    static World mondo;
    Prop cr = CriptaA(100.0f, 100.0f);

    mondo.hasTumulo = true;
    mondo.chunks[0].active    = true;
    mondo.chunks[0].cx        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].cz        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].propCount = 1;
    mondo.chunks[0].props[0]  = cr;

    /* Dentro il vano non si viene spinti. Il punto e' preso FUORI dal centro
     * esatto, a 1,5 m: sul centro la prova passerebbe anche senza aver scritto
     * niente, perche' il cerchio cotto da 5,0 m a distanza zero non ha una
     * direzione in cui spingere - passerebbe per il motivo sbagliato. A 1,5 m
     * il cerchio cotto spinge fino a 5,35, e il controllo cade davvero finche'
     * il tumulo non e' scritto. */
    Vector3 dentro = { 101.5f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &dentro, 0.35f);
    Ok("dentro il vano non si viene spinti",
       fabsf(dentro.x - 101.5f) < 0.01f && fabsf(dentro.z - 100.0f) < 0.01f);

    /* E sul centro esatto, che e' dove nasce il boss. */
    Vector3 centro = { 100.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &centro, 0.35f);
    Ok("sul centro esatto, dove nasce il boss, non si viene spinti",
       fabsf(centro.x - 100.0f) < 0.01f && fabsf(centro.z - 100.0f) < 0.01f);

    /* Contro un masso si viene spinti fuori DAL MASSO, non dal tumulo: la
     * distanza dal centro resta grande, quella dal masso diventa il suo raggio
     * piu' il proprio. */
    CryptStone anelloC[CRYPT_MAX_MASSI];
    int nc = CryptRing(&cr, anelloC, CRYPT_MAX_MASSI);
    Vector3 addosso = { 100.0f + anelloC[0].dx, 0.0f, 100.0f + anelloC[0].dz };
    WorldResolveCollision(&mondo, &addosso, 0.35f);
    float dm = sqrtf((addosso.x - (100.0f + anelloC[0].dx)) * (addosso.x - (100.0f + anelloC[0].dx)) +
                     (addosso.z - (100.0f + anelloC[0].dz)) * (addosso.z - (100.0f + anelloC[0].dz)));
    Ok("dentro un masso si viene spinti fuori da QUEL masso",
       dm > CryptStoneRadius(&anelloC[0]) + 0.35f - 0.01f);
    Ok("l'anello ha prodotto dei massi", nc > 0);

    /* Senza tumulo si torna al raggio cotto nel mondo: 5,0 + 0,35. E' la
     * promessa del ripiego, e va provata o non e' una promessa. */
    mondo.hasTumulo = false;
    Vector3 kit = { 102.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &kit, 0.35f);
    float dk = sqrtf((kit.x - 100.0f) * (kit.x - 100.0f) +
                     (kit.z - 100.0f) * (kit.z - 100.0f));
    Ok("senza tumulo si torna al raggio cotto",
       fabsf(dk - (5.0f + 0.35f)) < 0.01f);
```

- [ ] **Step 2: eseguire e verificare che fallisca**

```bash
make prove
```

Atteso: `build/prove/tumulo` esce non-zero, con due righe `FALLITO`:

- **"dentro il vano non si viene spinti"** — oggi il cerchio cotto da 5,0 m
  spinge il punto a 1,5 m fino a 5,35, cioè fuori dall'anello;
- **"dentro un masso si viene spinti fuori da QUEL masso"** — nessuno conosce
  ancora i massi.

La riga sul **centro esatto** invece passa già, e passa per il motivo sbagliato:
a distanza zero la spinta non ha una direzione. Sta lì lo stesso perché dopo il
cambiamento deve continuare a valere, ed è quella che dice che il boss nasce
all'aperto.

- [ ] **Step 3: la spinta del giocatore**

In `WorldResolveCollision()`, subito dopo il blocco della torre e prima di
`if (p->radius <= 0.0f) continue;`:

```c
            /* Il tumulo non e' un cerchio ma quindici: il raggio 5,0 cotto nel
             * mondo lo si scavalca, come per il mastio, e per la stessa ragione
             * - ricuocere non aiuterebbe i mondi gia' salvati.
             *
             * Il centro resta LIBERO, ed e' voluto: il boss nasce esattamente
             * li', e a distanza zero la spinta non avrebbe una direzione. */
            if (p->type == PROP_CRYPT && w->hasTumulo) {
                CryptStone anello[CRYPT_MAX_MASSI];
                int n = CryptRing(p, anello, CRYPT_MAX_MASSI);
                for (int m = 0; m < n; m++) {
                    float sx = p->pos.x + anello[m].dx * p->scale;
                    float sz = p->pos.z + anello[m].dz * p->scale;
                    float mdx = pos->x - sx, mdz = pos->z - sz;
                    float md2 = mdx * mdx + mdz * mdz;
                    float mrr = CryptStoneRadius(&anello[m]) * p->scale + radius;
                    if (md2 < mrr * mrr && md2 > 0.0001f) {
                        float md = sqrtf(md2);
                        float mpush = (mrr - md) / md;
                        pos->x += mdx * mpush;
                        pos->z += mdz * mpush;
                    }
                }
                continue;
            }
```

- [ ] **Step 4: il taglio della camera**

In `WorldCameraClip()`, prima del blocco `if (p->type == PROP_TOWER || p->type == PROP_CRYPT)`:

```c
            /* Quindici massi non sono una scatola: un cilindro per masso, con
             * lo stesso RayTrunk dei fusti degli alberi. */
            if (p->type == PROP_CRYPT && w->hasTumulo) {
                CryptStone anello[CRYPT_MAX_MASSI];
                int n = CryptRing(p, anello, CRYPT_MAX_MASSI);
                for (int m = 0; m < n; m++) {
                    Vector3 sc = { p->pos.x + anello[m].dx * p->scale, p->pos.y,
                                   p->pos.z + anello[m].dz * p->scale };
                    float rr = CryptStoneRadius(&anello[m]) * p->scale;
                    /* Alto quanto il masso: 2,8 di lato per un masso che sta
                     * fra 1,2 e 2,3 di altezza - si prende il caso alto, perche'
                     * una camera che passa sopra un masso basso non da'
                     * fastidio, una che entra in uno alto si'. */
                    if (RayTrunk(eye, dir, sc, rr, 2.4f * p->scale, best, &hitT)
                        && hitT < best) best = hitT;
                }
                continue;
            }
```

- [ ] **Step 5: eseguire le prove**

```bash
make && make prove
```

Atteso: le quattro righe nuove `ok`, zero avvisi.

- [ ] **Step 6: sabotare**

1. **la collisione ignora il tumulo**: in `WorldResolveCollision()`, cambiare la
   condizione in `if (false)` — deve cadere "dentro un masso si viene spinti
   fuori da QUEL masso";
2. **il raggio del masso al posto del suo diametro**: in `CryptStoneRadius()`,
   togliere il `* 0.5f` — massi larghi il doppio arrivano a 2,8 m di raggio da
   un anello a 5,5, quindi il vano libero scende a 2,7: deve cadere "il vano
   interno tiene il boss e chi lo combatte" nel Task 2, e "dentro il vano non si
   viene spinti" qui;
3. **la spinta usa il centro della cripta invece del masso**: usare `p->pos.x` e
   `p->pos.z` al posto di `sx` e `sz` — deve cadere "dentro il vano non si viene
   spinti", perché quindici cerchi concentrici sul centro chiudono il vano.

- [ ] **Step 7: verificare in gioco, misurando**

Con lo stesso binario strumentato del Task 3, invece di uno scatto:

```c
    Vector3 q = g->world.cryptPos;
    WorldResolveCollision(&g->world, &q, 0.35f);
    TraceLog(LOG_INFO, "PROVA: dal centro della cripta la spinta muove di %.2f m",
             (double)Vector3Distance(q, g->world.cryptPos));
```

Atteso: **0,00 m**. È il numero che dice che il boss nasce all'aperto.

- [ ] **Step 8: commit**

```bash
git add src/world.c tools/prove/tumulo.c
git commit -m "Il tumulo si tocca masso per masso, e il centro resta libero"
```

---

### Task 5: La statua a fianco dell'ingresso

**Files:**
- Modify: `src/worldtypes.h` — un valore nell'enum
- Modify: `src/world.c` — `BUILD_FILES`, `gBuildMat`, `PreparaPezzo()`,
  il caricamento dei pezzi facoltativi, e `CryptDraw()`
- Modify: `src/world.h` — `keepScale` diventa `partScale[]`

**Interfaces:**
- Consumes: `CaricaPezzo()`, `PreparaPezzo()`, `PlacePart()`, `CryptRing()`.
- Produces: `BUILD_STATUE`; `w->partScale[BUILD_PART_COUNT]` al posto di
  `w->keepScale`.

- [ ] **Step 1: il pezzo nell'enum**

In `src/worldtypes.h`, aggiungere `BUILD_STATUE` **dopo** `BUILD_KEEP`:

```c
typedef enum {
    BUILD_WALL, BUILD_DOOR, BUILD_WINDOW, BUILD_ROOF, BUILD_FLOOR, BUILD_STAIRS,
    BUILD_TOWER_BASE, BUILD_TOWER_MID, BUILD_TOWER_TOP, BUILD_TOWER_ROOF,
    BUILD_KIT_COUNT,
    BUILD_KEEP = BUILD_KIT_COUNT,
    BUILD_STATUE,
    BUILD_PART_COUNT
} BuildPart;
```

I pezzi facoltativi sono ora **due**, e restano quelli da `BUILD_KIT_COUNT` in
poi: il conto non cambia forma.

- [ ] **Step 2: le due righe delle tabelle**

In `src/world.c`, in `BUILD_FILES`, dopo la riga di `BUILD_KEEP`:

```c
    /* gothic_statue, misurata il 2026-09-08: 1,48 x 1,74 x 1,56 m, 1 mesh,
     * 23.314 vertici, un materiale PBR. E' un file intero - pezzo -1 - ma con
     * una taglia dichiarata, perche' non sta sulla griglia di BUILD_CELL: e'
     * l'unico oggetto del catalogo che dica "tomba" invece di "sasso". */
    [BUILD_STATUE]      = { "assets/models/statua.glb",                        -1,
                            { 0 }, 1.74f, true },
```

E in `gBuildMat`:

```c
    [BUILD_STATUE]      = { NULL,          0.0f, 0, true  },
```

- [ ] **Step 3: `keepScale` diventa `partScale[]`**

In `src/world.h`, sostituire il campo `keepScale`:

```c
    /* Il moltiplicatore che porta ogni pezzo alla sua taglia dichiarata. Per
     * pezzo e non per il solo mastio: i pezzi facoltativi sono due, e il
     * secondo non deve sovrascrivere i numeri del primo. */
    float  partScale[BUILD_PART_COUNT];
```

In `src/world.c`, dentro `PreparaPezzo()`, sostituire il blocco guardato da
`if (i == BUILD_KEEP)`:

```c
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
```

E in `DrawKeep()`, `w->keepScale` diventa `w->partScale[BUILD_KEEP]`.

- [ ] **Step 4: la taglia metrica anche per un file intero**

In `src/world.c`, subito prima del ciclo che carica i pezzi facoltativi:

```c
/* La scala di un pezzo che e' un FILE INTERO e dichiara una taglia in metri.
 * I pezzi dei kit non la dichiarano - stanno sulla griglia di BUILD_CELL e
 * hanno 'voluto' a zero - e per loro la scala resta 1. */
static float ScalaPezzoIntero(const Model *m, int i)
{
    if (BUILD_FILES[i].voluto <= 0.0f) return 1.0f;
    MeshGroup g = { 0, m->meshCount, GetModelBoundingBox(*m) };
    return MeshGroupScale(&g, BUILD_FILES[i].voluto, BUILD_FILES[i].perAltezza);
}
```

- [ ] **Step 5: caricare i pezzi facoltativi in un ciclo**

In `src/world.c`, sostituire il blocco `if (CaricaPezzo(w, BUILD_KEEP)) { ... }`
in coda a `LoadBuildParts()`:

```c
    /* I pezzi facoltativi: quelli dopo i dieci dei kit. NON entrano nel "tutti o
     * nessuno", che esiste perche' mezza casa e' peggio di una scatola - mentre
     * una torre Kenney e una cripta senza statua sono cose intere e giuste. */
    for (int i = BUILD_KIT_COUNT; i < BUILD_PART_COUNT; i++) {
        if (!CaricaPezzo(w, i)) continue;
        w->buildLoaded[i] = true;
        LightApplyToModel(&w->buildPart[i]);

        if (BUILD_FILES[i].pezzo >= 0) {
            if (!PreparaPezzo(w, i)) {
                TraceLog(LOG_INFO, "WORLD: pezzo %d non preparato, si ripiega", i);
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
```

- [ ] **Step 6: posarla di fianco al varco**

In `CryptDraw()`, dopo il ciclo dei massi e prima di `return messi;`:

```c
    /* La statua sta DI FIANCO al varco, non in mezzo: l'ingresso e' il punto in
     * cui il giocatore corre, e una statua nel mezzo si prende una spallata.
     * Sfalsata di mezzo posto oltre il bordo del varco, e arretrata di 1,6 m
     * dall'anello, girata verso il centro. */
    if (w->buildLoaded[BUILD_STATUE]) {
        float h = CryptHash(p, 0, 151);
        int primoVuoto = (int)(h * (float)CRYPT_POSTI);
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
```

`PlacePart()` accoda al lotto quando c'è e disegna quando non c'è, quindi la
statua segue la stessa strada dei massi in tutti e due i casi.

- [ ] **Step 7: compilare e leggere**

```bash
make && make prove
timeout 12 ./frostmark > /tmp/statua.log 2>&1
grep -iE "pezzo assets|cripta" /tmp/statua.log
```

Atteso, fra le altre:

```
WORLD: pezzo assets/models/statua.glb x1.00, a lotti
```

`x1.00` perché la taglia dichiarata, 1,74 m, è l'altezza vera dell'asset.

- [ ] **Step 8: guardarla, e verificare che non sia in mezzo**

Con il binario strumentato: la statua deve stare **a lato** dell'apertura, non
dentro. Camminare nel varco non deve farci sbattere contro.

- [ ] **Step 9: verificare che senza la statua l'anello regga**

```bash
mv assets/models/statua.gltf /tmp/statua-via
timeout 10 ./frostmark > /tmp/senza2.log 2>&1
grep -icE "statua" /tmp/senza2.log
mv /tmp/statua-via assets/models/statua.gltf
```

Atteso: **zero** righe sulla statua, nessun avviso, e il tumulo disegnato lo
stesso.

- [ ] **Step 10: commit**

```bash
git add src/worldtypes.h src/world.c src/world.h
git commit -m "Una statua di fianco all'ingresso, e i pezzi facoltativi in un ciclo"
```

---

### Task 6: Scrivere quello che si è imparato

**Files:**
- Modify: `docs/01-architettura.md` — la sezione *Pezzi indicizzati*
- Modify: `docs/03-asset-pubblici.md` — la tabella degli asset misurati
- Modify: `docs/06-stato-e-prossimi-passi.md` — la tabella, la domanda E, le prove

- [ ] **Step 1: `docs/01`**

In coda a *Pezzi indicizzati*, un paragrafo **La cripta è una ricetta**: che
`CryptRing()` produce i massi dalla posizione e la leggono in tre; che il centro
resta libero e perché (il boss nasce lì, e la spinta a distanza zero non ha una
direzione); che `hasTumulo` distingue un set da un modello singolo, così un
asset a una variante torna a comportarsi come prima.

- [ ] **Step 2: `docs/03`**

Aggiungere alla tabella degli asset misurati:

| asset Poly Haven | triangoli | vertici/mesh | alfa | si usa? |
|---|---|---|---|---|
| `rock_moss_set_01` | 63.127 | 8.538 | opaco | **sì — è il tumulo della cripta**, set di sei |
| `gothic_statue` | 27.739 | 23.314 | opaco | **sì — la statua della cripta** |
| `namaqualand_boulders_01` | 40.850 | 12.909 | opaco | **no**: sono ciottoli da 0,34 m |
| `namaqualand_rocks_01` | 85.716 | 12.996 | opaco | **no**: è ghiaia da 0,22 m |

E una riga sotto *Cosa non ha un equivalente texturizzato*: che **una cripta non
esiste** nei 521 modelli del catalogo, cercata su nomi, categorie e tag; e che
due asset chiamati `boulders` e `rocks` stanno sotto i 35 cm — **il nome non è
una misura**.

- [ ] **Step 3: `docs/06`**

- nella tabella dei pezzi, la cripta passa da *aperto* a **fatto**;
- la domanda **E** si chiude, con le due affermazioni che il lavoro ha corretto:
  la proiezione non era «già preparata» — non è su quel percorso — e la cripta
  del kit ha **33 coppie UV distinte su 1.028 vertici**, cioè una tavolozza;
- *Cosa è realistico in gioco, oggi* prende la riga del tumulo;
- le prove passano da **otto** a **nove**, e quelle senza GPU da tre a
  **quattro**: `scale`, `varianti`, `mastio`, `tumulo`;
- nei sabotaggi, il caso nuovo: **una prova che verifica un varco senza
  verificare che l'anello sia chiuso altrove** passerebbe anche con un anello
  tutto buchi, purché i buchi siano uno per volta.

- [ ] **Step 4: verificare che i documenti non mentano**

```bash
make && make prove && make valida
ls tools/prove/*.c | wc -l
```

Atteso: nove file, prove passate, e ogni numero ricontrollato contro il registro
del gioco — non contro questo piano.

- [ ] **Step 5: commit**

```bash
git add docs/
git commit -m "I documenti dicono che la cripta e' un luogo"
```

---

## Note per chi esegue

**L'ordine conta.** Il Task 2 aggiunge una funzione provata senza toccare il
gioco; il 3 cambia quello che si vede; il 4 la collisione; il 5 la statua. Ognuno
si può guardare da solo.

**`CryptRing()` non deve mai dipendere da altro che dal `Prop`.** Se un giorno
gli serve il mondo, il terreno o un contatore, la ricetta smette di essere una
funzione della posizione e disegno e collisione possono divergere. La quota sta
fuori apposta.

**Non ricuocere il mondo.** Il raggio 5,0 in `tools/worldgen.c` resta dov'è: i
mondi già salvati non si aggiornerebbero, ed è per questo che `hasTumulo`
scavalca invece di sostituire.

**La statua è sacrificabile.** Se il Task 5 si rivela più lungo del previsto,
l'anello senza statua è un risultato intero e il gioco funziona: costa 3,98 MB e
serve a far leggere il tumulo come una tomba, non a farlo funzionare.
