# Il sottobosco — piano di implementazione

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** il suolo della foresta smette di essere erba vuota e diventa
sottobosco — ceppi, tronchi caduti, radici affioranti, rami secchi e scaglie di
corteccia — mentre gli alberi restano stilizzati, perché il catalogo non ne ha di
grandi a un peso utilizzabile.

**Architecture:** i cinque tipi nuovi hanno un comportamento comune e diverso dai
sette esistenti, quindi non sono sei modifiche a testa ma **una riga a testa** in
una tabella condivisa, `src/propdefs.c`, letta sia dal gioco sia dal `baker`.
Nascono da una **seconda passata** di generazione, indipendente da quella della
vegetazione: il sottobosco non è un'alternativa a un albero, è ciò che sta fra
gli alberi.

**Tech Stack:** C99, raylib 5.5, glTF di Poly Haven, `make` + `make prove` +
`make mondo`.

**Spec:** `docs/superpowers/specs/2026-09-08-sottobosco-design.md`

## Global Constraints

- **Zero avvisi.** `-Wall -Wextra`, Linux e Windows.
- **Il gioco funziona senza `assets/`.** Per il sottobosco la regola è più
  stretta del solito: **niente asset, niente prop** — non si disegna e **non è
  solido**. Senza la seconda metà, il giocatore sbatterebbe contro tronchi
  invisibili, perché il raggio sta nel mondo cotto.
- **Il mondo si ricuoce**, e questa volta è necessario: `make mondo`. Non rompe
  salvataggi — `assets/world/` non è versionato e `SaveData` non indicizza i
  prop — ma va detto nei documenti.
- **Tetto dei vertici: 65.535 per primitiva.** Il peggiore dei cinque ne ha
  46.112.
- **`MAX_PROPS_PER_CHUNK` è 160**, e `baker` conta i `fullChunks`: quel numero
  deve restare **zero**. Se non lo resta si alza il tetto o si abbassa la
  densità, e la scelta si fa sul numero misurato, non a occhio.
- **Le prove si verificano sabotandole.**
- **Commenti e messaggi di commit in italiano**, che dicono *perché*.
- **Numeri misurati:** `tree_stump_01` 1,43 × 0,57 × 1,59 / 22.472 vert;
  `dead_tree_trunk_02` 4,05 × 1,06 × 1,06 / 46.112; `pine_roots` 1,74 × 0,15 ×
  0,83 / 43.897 / **2 gruppi**; `dry_branches_medium_01` 0,32 × 0,34 × 1,30 /
  4.252 / **3 gruppi**; `bark_debris_01` 0,19 × 0,10 × 0,61 / 39.748 / **4
  gruppi**. Trenta MB in tutto.

---

### Task 1: Scaricare i cinque asset

**Files:**
- Nessuna modifica al codice: il sottocomando `polyhaven` fa già tutto.
- Modify: `assets/CREDITS.md` (lo scrive lo script)

**Interfaces:**
- Produces: i cinque percorsi che il Task 2 nomina nella tabella —
  `assets/models/{ceppo,tronco,radici,rami,corteccia}.gltf`.

- [ ] **Step 1: scaricarli**

```bash
./tools/fetch_assets.sh polyhaven tree_stump_01          ceppo
./tools/fetch_assets.sh polyhaven dead_tree_trunk_02     tronco
./tools/fetch_assets.sh polyhaven pine_roots             radici
./tools/fetch_assets.sh polyhaven dry_branches_medium_01 rami
./tools/fetch_assets.sh polyhaven bark_debris_01         corteccia
```

I nomi italiani sono quelli che la tabella cercherà: la tabella dichiara
`assets/models/ceppo.glb`, e `TrovaModello()` prova prima il `.glb` e poi
l'altra estensione, quindi trova il `.gltf` scaricato.

- [ ] **Step 2: verificare**

```bash
ls -la assets/models/{ceppo,tronco,radici,rami,corteccia}.gltf
grep -cE "ceppo|tronco|radici|rami|corteccia" assets/CREDITS.md
du -sh assets/models
```

Atteso: cinque `.gltf`, **cinque** righe di crediti, e `assets/models` cresciuto
di circa 30 MB.

- [ ] **Step 3: commit**

```bash
git add assets/CREDITS.md
git commit -m "I cinque pezzi del sottobosco, dal catalogo"
```

---

### Task 2: Una riga per prop di dettaglio

**Files:**
- Modify: `src/worldtypes.h` — cinque valori nell'enum `PropType`
- Create: `src/propdefs.h`
- Create: `src/propdefs.c`
- Create: `tools/prove/sottobosco.c`
- Modify: `Makefile` — `BAKER_SRCS` e una riga nel bersaglio `prove`

**Interfaces:**
- Consumes: `PropType` da `src/worldtypes.h`.
- Produces:
  - `typedef struct { PropType type; const char *file; float voluto; bool perAltezza; float maxDist; float raggio; float lunghezza; bool ombra; float frequenza; } PropDetail;`
  - `const PropDetail *PropDetailOf(PropType t)` — **NULL** per i sette tipi
    esistenti;
  - `PropType PropDetailPick(float r)` — scelta pesata, `r` in [0,1);
  - `extern const PropDetail gPropDetail[]; extern const int gPropDetailCount;`

- [ ] **Step 1: i cinque tipi nell'enum**

In `src/worldtypes.h`:

```c
/* I primi otto sono i prop "grandi": ognuno ha la sua riga in gExtProp e la sua
 * primitiva di riserva in DrawProp, e senza assets/ il gioco li disegna lo
 * stesso.
 *
 * I cinque dopo sono il SOTTOBOSCO, e si comportano diversamente: stanno tutti
 * in una tabella sola (src/propdefs.c) e non hanno una primitiva di riserva -
 * senza il loro asset non esistono, ne' disegnati ne' solidi. Una sfera
 * schiacciata al posto di una scaglia di corteccia sarebbe peggio del niente. */
typedef enum {
    PROP_TREE, PROP_PINE, PROP_ROCK, PROP_BUSH,
    PROP_HERB, PROP_HOUSE, PROP_TOWER, PROP_CRYPT,
    PROP_STUMP, PROP_LOG, PROP_ROOTS, PROP_BRANCH, PROP_BARK,
    PROP_COUNT
} PropType;
```

- [ ] **Step 2: l'header**

Crea `src/propdefs.h`:

```c
/* ============================================================================
 * propdefs.h - I prop di DETTAGLIO in una tabella sola.
 *
 * Aggiungere un prop "grande" tocca sei punti: l'enum, gExtProp, il switch
 * delle distanze, il switch del ripiego procedurale, la lista di chi non fa
 * ombra, e l'emissione nel baker. Per i cinque del sottobosco sarebbero trenta
 * modifiche, e una di quelle chiederebbe di inventare una primitiva procedurale
 * per una scaglia di corteccia da 20 cm.
 *
 * Qui invece un pezzo di sottobosco e' UNA RIGA, e la stessa riga la leggono il
 * gioco - per caricare, disegnare e collidere - e il baker, per emettere.
 *
 * I sette tipi grandi NON stanno qui: PropDetailOf() torna NULL per loro, ed e'
 * quel NULL a distinguere le due regole.
 * ========================================================================== */
#ifndef PROPDEFS_H
#define PROPDEFS_H

#include "worldtypes.h"
#include <stdbool.h>

typedef struct {
    PropType    type;
    const char *file;
    float       voluto;      /* taglia in metri                              */
    bool        perAltezza;  /* su cosa si misura: false = lato XZ maggiore   */
    float       maxDist;     /* oltre, non si disegna                        */
    float       raggio;      /* collisione: 0 = si calpesta                   */
    float       lunghezza;   /* >0: oggetto allungato, due cerchi sull'asse   */
    bool        ombra;       /* proietta ombra?                              */
    float       frequenza;   /* quota fra i cinque; la somma vale 1           */
} PropDetail;

extern const PropDetail gPropDetail[];
extern const int        gPropDetailCount;

/* La riga di un tipo, o NULL se non e' un prop di dettaglio. */
const PropDetail *PropDetailOf(PropType t);

/* Quale dei cinque, per 'r' in [0,1). Pesata sulle frequenze. */
PropType PropDetailPick(float r);

#endif /* PROPDEFS_H */
```

- [ ] **Step 3: la tabella**

Crea `src/propdefs.c`:

```c
#include "propdefs.h"
#include <stddef.h>

/* Misurati il 2026-09-08 sui .gltf a 1k di Poly Haven, ingombri dagli accessori
 * con le trasformazioni dei nodi applicate.
 *
 * 'voluto' e' sul LATO XZ MAGGIORE per tutti: sono oggetti che stanno a terra e
 * si misurano in larghezza, non in altezza - la stessa ragione per cui l'erba
 * usa perAltezza = false.
 *
 * 'maxDist' viene dalla taglia. Una scaglia da 61 cm a 140 m e' meno di un
 * pixel e costa una riga d'istanza: con cinque tipi nuovi sparsi su tutta la
 * foresta, le distanze sono la difesa del fotogramma.
 *
 * Solido e' solo il TRONCO: 4,05 x 1,06 e' un ostacolo che si vede e si aggira.
 * Il ceppo no, ed e' deliberato - e' alto 0,57 e il gioco non ha un gradino, per
 * cui un ostacolo al ginocchio che ferma di netto sembra un difetto.
 *
 * Ombra solo a tronco e ceppo: gli altri tre sono piatti a terra e non
 * proiettano niente che si veda, come erba e cespugli. */
const PropDetail gPropDetail[] = {
    /*  tipo         file                            voluto  perAlt  maxDist  raggio  lungh   ombra  freq  */
    { PROP_BARK,   "assets/models/corteccia.glb",     0.61f, false,   40.0f,   0.00f,  0.00f, false, 0.30f },
    { PROP_BRANCH, "assets/models/rami.glb",          1.30f, false,   50.0f,   0.00f,  0.00f, false, 0.30f },
    { PROP_ROOTS,  "assets/models/radici.glb",        1.74f, false,   60.0f,   0.00f,  0.00f, false, 0.17f },
    { PROP_STUMP,  "assets/models/ceppo.glb",         1.43f, false,  100.0f,   0.00f,  0.00f, true,  0.13f },
    { PROP_LOG,    "assets/models/tronco.glb",        4.05f, false,  140.0f,   1.10f,  4.05f, true,  0.10f },
};

const int gPropDetailCount = (int)(sizeof gPropDetail / sizeof gPropDetail[0]);

const PropDetail *PropDetailOf(PropType t)
{
    for (int i = 0; i < gPropDetailCount; i++)
        if (gPropDetail[i].type == t) return &gPropDetail[i];
    return NULL;
}

/* Scelta pesata. Le frequenze sommano 1, ma non ci si appoggia: con r == 1 o con
 * una somma appena sotto per errore di virgola mobile si torna l'ultima riga
 * invece di leggere fuori dall'array. */
PropType PropDetailPick(float r)
{
    float acc = 0.0f;
    for (int i = 0; i < gPropDetailCount; i++) {
        acc += gPropDetail[i].frequenza;
        if (r < acc) return gPropDetail[i].type;
    }
    return gPropDetail[gPropDetailCount - 1].type;
}
```

- [ ] **Step 4: collegarlo al baker**

In `Makefile`, aggiungere `src/propdefs.c` a `BAKER_SRCS`:

```make
BAKER_SRCS := $(TOOL_DIR)/baker.c $(TOOL_DIR)/worldgen.c $(TOOL_DIR)/noise.c \
              $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c $(SRC_DIR)/fmath.c \
              $(SRC_DIR)/propdefs.c
```

Nel gioco entra da solo: `SRCS` è un `wildcard` su `src/*.c`.

- [ ] **Step 5: scrivere la prova**

Crea `tools/prove/sottobosco.c`:

```c
/* ============================================================================
 * sottobosco.c - I cinque prop di dettaglio stanno in una tabella, e la tabella
 * la leggono in due: il gioco per disegnare e collidere, il baker per emettere.
 * Se fosse incompleta o incoerente, un prop nascerebbe nel mondo cotto e non si
 * disegnerebbe - o peggio, sarebbe solido e invisibile.
 *
 * Niente OpenGL: e' una tabella, un po' di aritmetica e le due funzioni che la
 * leggono - la collisione del gioco e la generazione del baker. Includerli tutti
 * e tre nella stessa unita' di traduzione funziona: verificato compilando prima
 * di scrivere il piano, non ci sono simboli in conflitto.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "../worldgen.c"
#include "prova.h"

int main(void)
{
    /* --- La tabella e' completa ----------------------------------------- *
     * Ogni tipo dopo PROP_CRYPT e' un prop di dettaglio e deve avere la sua
     * riga. Una riga dimenticata darebbe un prop che il baker emette e il gioco
     * non sa disegnare, senza un avviso. */
    int mancanti = 0, senzaFile = 0, senzaTaglia = 0, senzaDistanza = 0;
    for (int t = PROP_STUMP; t < PROP_COUNT; t++) {
        const PropDetail *d = PropDetailOf((PropType)t);
        if (d == NULL) { mancanti++; continue; }
        if (d->file == NULL || d->file[0] == '\0') senzaFile++;
        if (d->voluto <= 0.0f) senzaTaglia++;
        if (d->maxDist <= 0.0f) senzaDistanza++;
    }
    Ok("ogni tipo di dettaglio ha la sua riga", mancanti == 0);
    Ok("ogni riga dichiara un file",            senzaFile == 0);
    Ok("ogni riga dichiara una taglia",         senzaTaglia == 0);
    Ok("ogni riga dichiara una distanza",       senzaDistanza == 0);
    Ok("la tabella ha esattamente cinque righe", gPropDetailCount == 5);

    /* E i sette grandi NON sono di dettaglio: e' quel NULL a decidere quale
     * regola si applica, e se un albero finisse nella tabella perderebbe la sua
     * primitiva di riserva. */
    int intrusi = 0;
    for (int t = PROP_TREE; t <= PROP_CRYPT; t++)
        if (PropDetailOf((PropType)t) != NULL) intrusi++;
    Ok("i prop grandi non stanno nella tabella", intrusi == 0);

    /* --- Le frequenze --------------------------------------------------- */
    float somma = 0.0f;
    for (int i = 0; i < gPropDetailCount; i++) somma += gPropDetail[i].frequenza;
    Ok("le frequenze sommano a uno", fabsf(somma - 1.0f) < 0.001f);

    /* La scelta pesata deve RISPETTARE le frequenze, non solo pescarle tutte.
     * Diecimila estrazioni su una griglia regolare: il conto atteso di ogni tipo
     * e' la sua frequenza per diecimila, con un decimo di tolleranza. */
    int conta[PROP_COUNT];
    for (int i = 0; i < PROP_COUNT; i++) conta[i] = 0;
    const int N = 10000;
    for (int k = 0; k < N; k++)
        conta[PropDetailPick(((float)k + 0.5f) / (float)N)]++;

    int tutti = 1, fuori = 0;
    for (int i = 0; i < gPropDetailCount; i++) {
        int atteso = (int)(gPropDetail[i].frequenza * (float)N);
        int letto  = conta[gPropDetail[i].type];
        if (letto == 0) tutti = 0;
        if (abs(letto - atteso) > atteso / 10) fuori++;
    }
    Ok("la scelta pesca tutti e cinque i tipi", tutti);
    Ok("e li pesca nelle proporzioni dichiarate", fuori == 0);

    /* Il tronco deve restare RARO: e' l'unico solido, quindi la sua frequenza e'
     * anche quella degli ostacoli. Un bosco con un tronco caduto ogni due passi
     * non e' un bosco. */
    Ok("il tronco e' il piu' raro dei cinque",
       conta[PROP_LOG] * 2 < conta[PROP_BARK]);

    /* Gli estremi non devono leggere fuori dall'array. */
    Ok("r = 0 da' un tipo valido",
       PropDetailOf(PropDetailPick(0.0f)) != NULL);
    Ok("r appena sotto 1 da' un tipo valido",
       PropDetailOf(PropDetailPick(0.999999f)) != NULL);
    Ok("r = 1 non esce dall'array",
       PropDetailOf(PropDetailPick(1.0f)) != NULL);

    /* --- Solido e allungato --------------------------------------------- *
     * Chi ha una lunghezza dichiarata deve avere anche un raggio, o i due cerchi
     * della collisione avrebbero raggio zero e il tronco si attraverserebbe. */
    int incoerenti = 0;
    for (int i = 0; i < gPropDetailCount; i++)
        if (gPropDetail[i].lunghezza > 0.0f && gPropDetail[i].raggio <= 0.0f)
            incoerenti++;
    Ok("chi e' allungato e' anche solido", incoerenti == 0);

    /* E i due cerchi devono coprire il tronco senza lasciare buchi: stanno a
     * meta' lunghezza meno il raggio, quindi il raggio non puo' essere meno di
     * un quarto della lunghezza o in mezzo resterebbe un varco. */
    int scoperti = 0;
    for (int i = 0; i < gPropDetailCount; i++) {
        const PropDetail *d = &gPropDetail[i];
        if (d->lunghezza <= 0.0f) continue;
        float centro = d->lunghezza * 0.5f - d->raggio;
        if (centro > d->raggio) scoperti++;    /* i due cerchi non si toccano */
    }
    Ok("i due cerchi del tronco non lasciano un buco in mezzo", scoperti == 0);

    return ProveEsito();
}
```

- [ ] **Step 6: la riga nel Makefile**

Nel bersaglio `prove`, dopo il blocco di `tumulo`:

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/sottobosco.c \
	      $(SRC_DIR)/fmath.c $(SRC_DIR)/light.c $(SRC_DIR)/instancing.c \
	      $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c $(SRC_DIR)/propdefs.c \
	      $(TOOL_DIR)/noise.c \
	      $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/sottobosco
```

`meshgroup.c`, `world.c` e `worldgen.c` **non** si collegano: la prova li
include. `propdefs.c` invece sì, perche' `world.c` ne prende solo l'header.
`noise.c` serve a `worldgen.c`.

La riga e' gia' quella definitiva: i Task 4 e 5 aggiungono controlli a questa
prova, non la ricostruiscono.

- [ ] **Step 7: costruire ed eseguire**

```bash
make && make prove
```

Atteso: quindici righe `ok` in `build/prove/sottobosco`, zero avvisi, e tutte le
altre prove come prima. Il gioco compila ma non è cambiato: nessuno emette
ancora i prop nuovi.

- [ ] **Step 8: sabotare**

1. **togliere la riga di `PROP_ROOTS`** dalla tabella — deve cadere "ogni tipo di
   dettaglio ha la sua riga" e "la tabella ha esattamente cinque righe";
2. **dare a tutti frequenza 0,20** — deve cadere "e li pesca nelle proporzioni
   dichiarate" e "il tronco è il più raro dei cinque". Se non cade, il controllo
   sta solo contando che compaiano tutti;
3. **in `PropDetailPick()`, tornare sempre `gPropDetail[0].type`** — deve cadere
   "la scelta pesca tutti e cinque i tipi";
4. **togliere il ritorno di sicurezza** e lasciare che il ciclo finisca senza
   `return` (con `r = 1`) — il compilatore avvisa, ed è il punto: l'estremo va
   gestito;
5. **mettere `raggio` a 0 sul tronco** lasciando la lunghezza — deve cadere "chi
   è allungato è anche solido";
6. **mettere `raggio` 0,50 sul tronco** — 4,05/2 − 0,50 = 1,52 > 0,50: deve
   cadere "i due cerchi del tronco non lasciano un buco in mezzo".

- [ ] **Step 9: commit**

```bash
git add src/worldtypes.h src/propdefs.h src/propdefs.c tools/prove/sottobosco.c Makefile
git commit -m "Un prop di dettaglio e' una riga, non sei modifiche"
```

---

### Task 3: Caricare, disegnare, e non disegnare

**Files:**
- Modify: `src/world.c` — `LoadExtProps()`, `PropMaxDist()`, `DrawProp()`,
  `WorldDrawShadowCasters()`

**Interfaces:**
- Consumes: `PropDetailOf()`, `gPropDetail`.
- Produces: nessuna funzione nuova; i quattro punti leggono la tabella.

- [ ] **Step 1: il caricamento consulta la tabella**

In `src/world.c`, in cima a `LoadExtProps()`, sostituire le prime righe del
ciclo:

```c
    for (int t = 0; t < PROP_COUNT; t++) {
        /* Un prop di dettaglio prende file e taglia dalla sua riga; i sette
         * grandi restano su gExtProp. PropDetailOf torna NULL per loro, ed e'
         * quel NULL a scegliere quale tabella si legge. */
        const PropDetail *det = PropDetailOf((PropType)t);
        const char *decl = det ? det->file       : gExtProp[t].file;
        float voluto     = det ? det->voluto     : gExtProp[t].voluto;
        bool  perAltezza = det ? det->perAltezza : gExtProp[t].perAltezza;
        if (decl == NULL) continue;

        char alt[256];
        const char *file = TrovaModello(decl, alt, (int)sizeof alt);
        if (file == NULL) continue;
```

e più sotto, dove la scala si calcola, `gExtProp[t].voluto` e
`gExtProp[t].perAltezza` diventano `voluto` e `perAltezza`. La riga di `TraceLog`
finale usa `voluto` allo stesso modo.

- [ ] **Step 2: la distanza viene dalla tabella**

Sostituire `PropMaxDist()`:

```c
/* Distanza massima di disegno per tipo. Un cespuglio a 300 m e' un pixel che
 * costa quanto una casa: ogni prop e' una o due chiamate di disegno, e con
 * ~6000 props caricati il conto misurato era 25 ms per fotogramma, cioe' tutto
 * il budget. Gli alberi restano visibili da lontano perche' danno la forma del
 * paesaggio; case e torri sono punti di riferimento e non si tagliano.
 *
 * Per il sottobosco la distanza sta nella sua riga: sono cinque tipi sparsi su
 * tutta la foresta, e una scaglia da 61 cm a 140 m e' meno di un pixel. */
static float PropMaxDist(int type)
{
    const PropDetail *d = PropDetailOf((PropType)type);
    if (d != NULL) return d->maxDist;

    switch (type) {
        case PROP_HERB:
        case PROP_BUSH: return 80.0f;
        case PROP_ROCK: return 140.0f;
        case PROP_TREE:
        case PROP_PINE: return 260.0f;
        default:        return 400.0f;
    }
}
```

- [ ] **Step 3: niente ripiego procedurale**

In `DrawProp()`, subito prima dello `switch (p->type)`:

```c
    /* Il sottobosco non ha una primitiva di riserva, ed e' una scelta: una sfera
     * schiacciata al posto di una scaglia di corteccia e' peggio del niente.
     * Senza il suo asset il prop semplicemente non c'e'. La collisione fa lo
     * stesso conto, o si sbatterebbe contro un tronco invisibile. */
    if (PropDetailOf(p->type) != NULL) return;

    switch (p->type) {
```

- [ ] **Step 4: chi proietta ombra**

In `WorldDrawShadowCasters()`, sostituire la riga che salta erba e cespugli:

```c
            /* Un ciuffo d'erba e un fiore non proiettano niente che si veda, e
             * nel bosco sono la maggioranza dei prop: saltarli dimezza il
             * passaggio senza togliere un'ombra che qualcuno noterebbe. Per il
             * sottobosco lo dice la sua riga: tronco e ceppo si', il resto e'
             * piatto a terra. */
            if (p->type == PROP_HERB || p->type == PROP_BUSH) continue;
            const PropDetail *det = PropDetailOf(p->type);
            if (det != NULL && !det->ombra) continue;
```

- [ ] **Step 5: compilare e leggere il registro**

```bash
make && make prove
timeout 12 ./frostmark > /tmp/sotto.log 2>&1
grep -iE "modello esterno" /tmp/sotto.log
```

Atteso: cinque righe nuove, e i numeri delle varianti che confermano le misure —
`radici.gltf (2 mesh, 2 varianti…)`, `rami.gltf (3 mesh, 3 varianti…)`,
`corteccia.gltf (4 mesh, 4 varianti…)`, e una variante sola per ceppo e tronco.

Il gioco è ancora identico: nessuno emette i prop nuovi. È il Task 6 che li fa
nascere.

- [ ] **Step 6: commit**

```bash
git add src/world.c
git commit -m "Il sottobosco si carica dalla sua tabella, e senza asset non c'e'"
```

---

### Task 4: Il tronco è due cerchi, non uno

**Files:**
- Modify: `src/world.c` — `WorldResolveCollision()`
- Modify: `tools/prove/sottobosco.c`

**Interfaces:**
- Consumes: `PropDetailOf()`, `w->hasExtProp[]`.
- Produces: `static int PropDetailCircles(const World *w, const Prop *p, Vector3 *out, float *raggio)`
  — torna 0, 1 o 2 cerchi di collisione per un prop di dettaglio.

- [ ] **Step 1: scrivere la prova che fallisce**

In `tools/prove/sottobosco.c`, prima di `return ProveEsito();`:

```c
    /* --- I cerchi del tronco -------------------------------------------- *
     * Un tronco e' lungo 4,05 m e spesso 1,06: un cerchio solo sul suo spessore
     * lascerebbe attraversare le punte, uno che lo copre tutto sarebbe un muro
     * invisibile largo quattro metri. Due cerchi sull'asse, orientati dalla
     * rotazione del prop.
     *
     * World e' grosso: static, o si rischia la pila. */
    static World mondo;
    Prop tronco = { 0 };
    tronco.pos   = (Vector3){ 50.0f, 0.0f, 50.0f };
    tronco.scale = 1.0f;
    tronco.rot   = 0.0f;
    tronco.type  = PROP_LOG;

    Vector3 c[2];
    float raggio = 0.0f;

    /* Senza il modello caricato non ci sono cerchi: niente asset, niente prop.
     * E' la meta' della regola che si dimentica, e senza di lei il giocatore
     * sbatte contro tronchi invisibili. */
    mondo.hasExtProp[PROP_LOG] = false;
    Ok("senza il modello il tronco non e' solido",
       PropDetailCircles(&mondo, &tronco, c, &raggio) == 0);

    mondo.hasExtProp[PROP_LOG] = true;
    int n = PropDetailCircles(&mondo, &tronco, c, &raggio);
    Ok("con il modello il tronco da' due cerchi", n == 2);
    Ok("del raggio dichiarato", fabsf(raggio - 1.10f) < 0.01f);

    /* I due cerchi stanno sull'asse del tronco, simmetrici sul centro. */
    float dx = c[1].x - c[0].x, dz = c[1].z - c[0].z;
    float d  = sqrtf(dx * dx + dz * dz);
    Ok("i cerchi coprono la lunghezza del tronco",
       fabsf(d - (4.05f - 2.0f * 1.10f)) < 0.01f);
    Ok("e sono simmetrici sul centro del prop",
       fabsf((c[0].x + c[1].x) * 0.5f - 50.0f) < 0.01f &&
       fabsf((c[0].z + c[1].z) * 0.5f - 50.0f) < 0.01f);

    /* Ruotato di 90 gradi l'asse gira con lui: se i cerchi restassero sull'asse
     * X il tronco bloccherebbe dalla parte sbagliata, e a occhio non si vede. */
    tronco.rot = 90.0f;
    PropDetailCircles(&mondo, &tronco, c, &raggio);
    float dx2 = c[1].x - c[0].x, dz2 = c[1].z - c[0].z;
    Ok("ruotato, i cerchi ruotano con lui",
       fabsf(dx2) < 0.01f && fabsf(fabsf(dz2) - (4.05f - 2.20f)) < 0.01f);

    /* Un prop di dettaglio non solido non da' cerchi. */
    Prop scaglia = tronco;
    scaglia.type = PROP_BARK;
    mondo.hasExtProp[PROP_BARK] = true;
    Ok("la corteccia si calpesta",
       PropDetailCircles(&mondo, &scaglia, c, &raggio) == 0);
```

- [ ] **Step 2: eseguire e verificare che NON compili**

```bash
make prove
```

Atteso: errore, `PropDetailCircles` non dichiarata. La prova include già
`world.c` dal Task 2, quindi non c'è niente da riorganizzare: manca solo la
funzione.

- [ ] **Step 3: scrivere la funzione**

In `src/world.c`, prima di `WorldResolveCollision()`:

```c
/* I cerchi di collisione di un prop di dettaglio. Torna quanti ne ha scritti:
 * 0 se il prop si calpesta o se il suo modello non e' caricato, 1 se e' tondo,
 * 2 se e' allungato.
 *
 * Due cerchi e non uno per il tronco: e' lungo 4,05 m e spesso 1,06, quindi un
 * cerchio sullo spessore lascerebbe attraversare le punte e uno che lo copre
 * tutto sarebbe un muro invisibile largo quattro metri. Stanno sull'asse del
 * prop, che ruota con lui.
 *
 * Lo zero quando il modello manca e' la meta' della regola che si dimentica: il
 * raggio sta nel MONDO COTTO, quindi senza questo controllo si sbatterebbe
 * contro tronchi invisibili. */
static int PropDetailCircles(const World *w, const Prop *p, Vector3 *out,
                             float *raggio)
{
    const PropDetail *d = PropDetailOf(p->type);
    if (d == NULL || d->raggio <= 0.0f) return 0;
    if (!w->hasExtProp[p->type]) return 0;

    *raggio = d->raggio * p->scale;

    if (d->lunghezza <= 0.0f) { out[0] = p->pos; return 1; }

    /* Il centro di ogni cerchio sta a meta' lunghezza meno il raggio, cosi' i
     * due coprono il tronco senza sporgere oltre le punte. */
    float off = (d->lunghezza * 0.5f - d->raggio) * p->scale;
    float a   = p->rot * DEG2RAD;
    float ux  = cosf(a), uz = -sinf(a);

    out[0] = (Vector3){ p->pos.x - ux * off, p->pos.y, p->pos.z - uz * off };
    out[1] = (Vector3){ p->pos.x + ux * off, p->pos.y, p->pos.z + uz * off };
    return 2;
}
```

- [ ] **Step 4: usarla nella spinta**

In `WorldResolveCollision()`, subito dopo il blocco della cripta e prima di
`if (p->radius <= 0.0f) continue;`:

```c
            /* Il sottobosco: la sua riga dice se e' solido, e il modello dice se
             * esiste. Il raggio cotto nel mondo non si legge - la tabella e'
             * l'unica fonte, cosi' cambiare un numero non chiede di ricuocere. */
            if (PropDetailOf(p->type) != NULL) {
                Vector3 cc[2];
                float rr = 0.0f;
                int nc = PropDetailCircles(w, p, cc, &rr);
                for (int m = 0; m < nc; m++) {
                    float ddx = pos->x - cc[m].x, ddz = pos->z - cc[m].z;
                    float dd2 = ddx * ddx + ddz * ddz;
                    float tot = rr + radius;
                    if (dd2 < tot * tot && dd2 > 0.0001f) {
                        float dd = sqrtf(dd2);
                        float push = (tot - dd) / dd;
                        pos->x += ddx * push;
                        pos->z += ddz * push;
                    }
                }
                continue;
            }
```

- [ ] **Step 5: eseguire le prove**

```bash
make && make prove
```

Atteso: le sette righe nuove `ok`, zero avvisi.

- [ ] **Step 6: sabotare**

1. **togliere il controllo su `hasExtProp`** — deve cadere "senza il modello il
   tronco non è solido". È il sabotaggio che protegge dai tronchi invisibili;
2. **non ruotare l'asse** (usare `ux = 1, uz = 0`) — deve cadere "ruotato, i
   cerchi ruotano con lui";
3. **mettere `off = d->lunghezza * 0.5f`** senza togliere il raggio — deve cadere
   "i cerchi coprono la lunghezza del tronco": i cerchi sporgerebbero oltre le
   punte di 1,10 m per parte;
4. **tornare 1 cerchio invece di 2** — deve cadere "con il modello il tronco dà
   due cerchi".

- [ ] **Step 7: commit**

```bash
git add src/world.c tools/prove/sottobosco.c
git commit -m "Il tronco caduto e' due cerchi sul suo asse"
```

---

### Task 5: La seconda passata

**Files:**
- Modify: `tools/worldgen.c` — `GenChunkProps()`
- Modify: `tools/prove/sottobosco.c`

**Interfaces:**
- Consumes: `PropDetailPick()`, `PropDetailOf()`, `GenBiome()`, `GenHeight()`,
  `GenNormal()`, `InsideAnyTown()`, `FmHash01()`.
- Produces: `static float SottoboscoDensity(Biome b)` in `tools/worldgen.c`.

- [ ] **Step 1: scrivere la prova che fallisce**

In `tools/prove/sottobosco.c`, prima di `return ProveEsito();`:

```c
    /* --- La densità della seconda passata -------------------------------- *
     * Il sottobosco vive dove ci sono alberi da cui derivare. Zero dove non ce
     * ne sono: un tronco caduto in mezzo all'oceano e' un difetto che si vede
     * una volta sola, per caso, dopo mesi. */
    Ok("in foresta il sottobosco e' fitto",  SottoboscoDensity(BIOME_FOREST) > 0.30f);
    Ok("in collina e' meno",                 SottoboscoDensity(BIOME_HILL) <
                                             SottoboscoDensity(BIOME_FOREST));
    Ok("in pianura e' raro",                 SottoboscoDensity(BIOME_PLAINS) < 0.12f);
    Ok("sull'oceano non ce n'e'",            SottoboscoDensity(BIOME_OCEAN) == 0.0f);
    Ok("sulla spiaggia non ce n'e'",         SottoboscoDensity(BIOME_BEACH) == 0.0f);
    Ok("sulla neve non ce n'e'",             SottoboscoDensity(BIOME_SNOW) == 0.0f);

    /* Nessuna densità sopra 1: sarebbe una cella su una, cioe' un tappeto. */
    int troppo = 0;
    for (int b = 0; b < BIOME_COUNT; b++)
        if (SottoboscoDensity((Biome)b) > 1.0f) troppo++;
    Ok("nessuna densità sfonda l'uno", troppo == 0);

    /* --- Il tetto per chunk ---------------------------------------------- *
     * La prima passata riempie 10x10 celle al 95,5% in foresta, la seconda 8x8
     * alla sua densità, e un chunk di villaggio ha 9 case piu' la torre. La
     * somma peggiore deve stare sotto MAX_PROPS_PER_CHUNK, o il baker tronca in
     * silenzio e sparisce roba dal mondo. */
    int peggiore = (int)(100.0f * 0.955f)
                 + (int)(64.0f * SottoboscoDensity(BIOME_FOREST))
                 + 10;
    Ok("il chunk peggiore sta sotto il tetto", peggiore < MAX_PROPS_PER_CHUNK);
```

- [ ] **Step 2: eseguire e verificare che NON compili**

```bash
make prove
```

Atteso: errore, `SottoboscoDensity` non dichiarata. La prova include già
`worldgen.c` dal Task 2: manca solo la funzione.

- [ ] **Step 3: la densità**

In `tools/worldgen.c`, prima di `GenChunkProps()`:

```c
/* Quanto sottobosco per cella, per bioma. Il sottobosco e' quello che un bosco
 * lascia cadere: dove non ci sono alberi non c'e'. Spiaggia, neve e oceano
 * restano a zero - un tronco caduto sulla sabbia e' un difetto che si vede una
 * volta sola, per caso, dopo mesi di gioco. */
static float SottoboscoDensity(Biome b)
{
    switch (b) {
        case BIOME_FOREST:   return 0.35f;
        case BIOME_HILL:     return 0.18f;
        case BIOME_PLAINS:   return 0.08f;
        case BIOME_MOUNTAIN: return 0.05f;
        default:             return 0.0f;
    }
}
```

- [ ] **Step 4: la passata**

In `GenChunkProps()`, dopo il ciclo della vegetazione e prima del blocco degli
edifici dei villaggi:

```c
    /* --- Seconda passata: il sottobosco ---------------------------------- *
     * NON entra nella catena qui sopra, e il motivo e' un numero: in foresta
     * quella catena riempie gia' il 95,5% delle celle, quindi mettercelo dentro
     * vorrebbe dire TOGLIERLO AGLI ALBERI. Ma il sottobosco non e'
     * un'alternativa a un albero: e' quello che sta fra gli alberi.
     *
     * Griglia 8x8 invece di 10x10 - celle da 8 m invece di 6,4 - perche' deve
     * essere sparso e non tappezzare. E sali d'hash tutti suoi: se li
     * condividesse con la vegetazione, i tronchi comparirebbero sempre accanto
     * agli stessi alberi, e il mondo sembrerebbe stampato con lo stampino. */
    const int SOTTO_CELLS = 8;
    for (int gz = 0; gz < SOTTO_CELLS; gz++) {
        for (int gx = 0; gx < SOTTO_CELLS; gx++) {
            int id = cx * 100019 + cz * 7927 + gz * SOTTO_CELLS + gx;
            float s1 = FmHash01(g->seed + 11u, id, 11);
            float s2 = FmHash01(g->seed + 12u, id, 12);
            float s3 = FmHash01(g->seed + 13u, id, 13);
            float s4 = FmHash01(g->seed + 14u, id, 14);
            float s5 = FmHash01(g->seed + 15u, id, 15);

            float x = ox + ((float)gx + s1) * (CHUNK_SIZE / SOTTO_CELLS);
            float z = oz + ((float)gz + s2) * (CHUNK_SIZE / SOTTO_CELLS);
            float h = GenHeight(g, x, z);
            if (h < SEA_LEVEL + 1.0f) continue;
            if (InsideAnyTown(g, x, z, -6.0f)) continue;

            /* Sul ripido non ci si posa: un tronco su una parete a 45 gradi
             * galleggia, e si vede. */
            Vector3 nrm = GenNormal(g, x, z);
            if (nrm.y < 0.80f) continue;

            if (s3 >= SottoboscoDensity(GenBiome(g, x, z))) continue;

            PropType t = PropDetailPick(s4);
            const PropDetail *d = PropDetailOf(t);
            /* Il raggio cotto resta a zero: per il sottobosco la collisione la
             * decide la tabella, cosi' cambiare un numero non chiede di
             * ricuocere il mondo. */
            EMIT(((Prop){ (Vector3){x, h, z}, 0.85f + s5 * 0.35f,
                          s4 * 360.0f, 0.0f, t, false }));
            (void)d;
        }
    }
```

`worldgen.c` deve includere `propdefs.h` in cima.

- [ ] **Step 5: cuocere e misurare**

```bash
make baker && make mondo
```

Il `baker` stampa quanti prop ha scritto e quanti chunk hanno toccato il tetto.

```bash
./baker --verifica
```

Atteso: **`fullChunks` a zero**. Se non lo è, il tetto morde e il mondo perde
roba in silenzio: si alza `MAX_PROPS_PER_CHUNK` in `src/config.h` oppure si
abbassa `SottoboscoDensity(BIOME_FOREST)`, e si ricuoce. La scelta si fa **sul
numero stampato**, non a occhio.

Annota il totale dei prop: prima erano **168.733**.

- [ ] **Step 6: guardarlo**

```bash
make && ./frostmark
```

Atteso a occhio, camminando in foresta: ceppi e rami sparsi fra gli alberi, un
tronco caduto ogni tanto da aggirare, e niente sulla spiaggia né sulla neve.

Se il sottobosco compare **sempre accanto agli stessi alberi**, i sali sono
condivisi con la vegetazione e vanno separati.

- [ ] **Step 7: commit**

```bash
git add tools/worldgen.c tools/prove/sottobosco.c
git commit -m "Il sottobosco nasce fra gli alberi, non al loro posto"
```

---

### Task 6: Scrivere quello che si è imparato

**Files:**
- Modify: `docs/01-architettura.md`, `docs/03-asset-pubblici.md`,
  `docs/06-stato-e-prossimi-passi.md`, `README.md`

- [ ] **Step 1: `docs/01`**

Una sezione **Prop di dettaglio**: che aggiungerne uno è una riga in
`src/propdefs.c` e non sei modifiche; che `PropDetailOf()` torna NULL per i sette
grandi ed è quel NULL a scegliere la regola; che il sottobosco non ha primitiva
di riserva e **senza asset non è nemmeno solido**, perché il raggio sta nel mondo
cotto; che il tronco è due cerchi sul suo asse e perché uno solo non basta.

- [ ] **Step 2: `docs/03`**

Nella tabella degli asset misurati, le cinque righe nuove. E una sezione
**Il catalogo, misurato tutto**: che si può leggere il conteggio dei vertici
dagli accessori del `.gltf` **senza scaricare il `.bin`** — 145 modelli vegetali
in pochi minuti — e che 109 stanno sotto il tetto ma i più grandi fra loro sono
alti un metro e mezzo.

- [ ] **Step 3: `docs/06`**

- la domanda **B** si chiude come **risolta diversamente**, non come *fatta*: gli
  alberi restano stilizzati, e lo spezzatore di mesh non si scrive perché
  sbloccherebbe un asset solo, alto 2,72 m e di specie desertica;
- il vincolo 2 (*Nessun albero del catalogo CC0 sta sotto quel tetto*) va
  **corretto**: 109 su 145 ci stanno, ma sono piccoli. Il vincolo vero è il peso;
- una riga nuova in *Cosa è realistico in gioco, oggi* per il sottobosco, con il
  totale dei prop misurato dopo la ricottura;
- le prove passano da nove a **dieci**, e quelle senza GPU da quattro a
  **cinque**;
- **`make mondo` è obbligatorio** dopo questo aggiornamento, e va detto dove chi
  aggiorna lo legge.

- [ ] **Step 4: `README.md`**

Se il README elenca i passi per partire, `make mondo` va menzionato come
necessario dopo questo cambiamento — controllare e aggiornare solo se lo elenca
davvero.

- [ ] **Step 5: verificare che i documenti non mentano**

```bash
make && make prove && make valida
ls tools/prove/*.c | wc -l
```

Atteso: dieci file, tutto verde, e ogni numero ricontrollato contro il registro
del gioco e l'uscita del `baker` — non contro questo piano.

- [ ] **Step 6: commit**

```bash
git add docs/ README.md
git commit -m "I documenti dicono perche' gli alberi restano di cartone"
```

---

## Note per chi esegue

**L'ordine conta, e il Task 5 va per ultimo fra quelli di codice.** I prop nuovi
non devono nascere finché disegno e collisione non sanno cosa farne: un prop
emesso senza il suo ramo di disegno sarebbe invisibile *e* solido, che è il
difetto peggiore di tutto questo lavoro.

**Il `fullChunks` va guardato davvero.** Il baker tronca in silenzio quando un
chunk supera il tetto: non c'è un errore, sparisce solo della roba. È l'unico
numero di questo piano che può rovinare il mondo senza dirlo.

**Non toccare il raggio cotto.** Il sottobosco dichiara la collisione nella
tabella e emette `radius = 0`: così cambiare il raggio di un tronco è cambiare un
numero in `propdefs.c`, non ricuocere quattromila chunk.
