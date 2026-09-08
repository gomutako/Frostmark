# Il mastio del forte — piano di implementazione

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** la torre di guardia al centro di ogni villaggio diventa `tower_round` di
`modular_fort_01` — una torre tonda in pietra fotogrammetrica, 15,84 × 13,50 ×
15,84 m — al posto dei quattro pezzi Kenney impilati.

**Architecture:** `BUILD_FILES` smette di essere un percorso e diventa una riga
che dice anche *quale pezzo* del file, per indice di gruppo. Il raggruppamento
delle mesh (`src/meshgroup.c`) separa già i venti pezzi del forte;
`InstModelCreateSubset()` disegna già un sottoinsieme di mesh. Il lavoro è
collegare le due cose, verificare che il pezzo preso sia quello atteso, e far
seguire la collisione alla nuova taglia da un numero solo.

**Tech Stack:** C99, raylib 5.5, glTF di Poly Haven, `make` + `make prove`.

**Spec:** `docs/superpowers/specs/2026-09-08-mastio-del-forte-design.md`

## Global Constraints

- **Zero avvisi.** Il progetto compila con `-Wall -Wextra` e non ne tollera
  nessuno: `make` deve restare pulito su Linux e Windows.
- **Il gioco funziona senza `assets/`.** Ogni asset mancante è un ripiego, mai
  un errore. Se `modular_fort_01` non c'è, la torre resta i quattro pezzi Kenney.
- **Il mondo cotto non si tocca.** La torre resta un prop con la stessa
  posizione, lo stesso tipo e lo stesso `radius` cotto. Nessun ricuocere.
- **Tetto dei vertici: 65.535 per primitiva.** Il pezzo peggiore del forte ne ha
  4.148, quindi il vincolo non morde qui, ma non va rimosso da nessun controllo.
- **Le prove si verificano sabotandole.** Una prova che passa dopo il sabotaggio
  non prova niente e va riscritta, non tenuta. Ogni compito che aggiunge una
  prova elenca i suoi sabotaggi e li esegue.
- **Commenti e messaggi di commit in italiano**, nello stile del repo: dicono
  *perché*, non *cosa*.
- **Numeri misurati, non stimati.** `tower_round` è il gruppo **12**, ingombro
  **15,84 × 13,50 × 15,84 m**, 2 mesh, 4.544 vertici, base a Y = 0. Il file ha
  45 primitive, 20 gruppi, 3 materiali PBR.

---

### Task 1: Scaricare il forte

**Files:**
- Modify: `tools/fetch_assets.sh` (intestazione dell'uso, e un blocco nuovo dopo
  quello `texture`)
- Nessun test automatico: il deliverable è la cartella `assets/models/fort/`.

**Interfaces:**
- Consumes: `tools/polyhaven_get.py`, che già crea le sottocartelle degli
  allegati — non va toccato.
- Produces: `assets/models/fort/modular_fort_01.gltf` + `.bin` + `textures/`,
  che il Task 4 nomina in `BUILD_FILES`.

- [ ] **Step 1: aggiungere il sottocomando all'elenco nell'intestazione**

In `tools/fetch_assets.sh`, nel blocco di commento iniziale, dopo la riga di
`texture` (se assente, dopo quella di `polyhaven`):

```sh
#        ./tools/fetch_assets.sh forte     scarica il kit modulare della fortezza
```

- [ ] **Step 2: aggiungere il blocco che scarica**

Subito dopo il blocco `if [ "${1:-}" = "texture" ]; then ... fi`:

```sh
# ---- kit modulari (Poly Haven) --------------------------------------------
#  Un kit modulare e' UN file con dentro molti pezzi: modular_fort_01 ne ha
#  venti, e 45 primitive in tutto. Il gioco li separa da se' - due mesh sono lo
#  stesso pezzo se i loro ingombri XZ si toccano - e ne indirizza uno per
#  indice, quindi qui non si spezza niente: si scarica il file com'e'.
#
#      ./tools/fetch_assets.sh forte
#
#  Lascia assets/models/fort/ con il .gltf, il suo .bin e le sue texture. La
#  cartella e' sua perche' il .gltf nomina il .bin e le texture per come stanno
#  nel pacchetto: rinominarli o spostarli lo romperebbe.
#
#  Senza questo file la torre resta quella del kit Kenney, e non e' un errore.
if [ "${1:-}" = "forte" ]; then
    for cmd in curl python3; do
        command -v "$cmd" >/dev/null 2>&1 || { echo "serve $cmd"; exit 1; }
    done

    ASSET="modular_fort_01"
    mkdir -p "$ASSETS/models/fort"

    echo "cerco $ASSET su polyhaven.com..."
    curl -sSL "https://api.polyhaven.com/files/$ASSET" -o "$ASSETS/.ph.json"
    python3 "$ROOT/tools/polyhaven_get.py" "$ASSETS/.ph.json" \
            "$ASSETS/models/fort" "$ASSET.gltf"
    rm -f "$ASSETS/.ph.json"

    if ! grep -q "assets/models/fort/$ASSET.gltf" "$ASSETS/CREDITS.md" 2>/dev/null; then
        printf '| assets/models/fort/%s.gltf | Poly Haven | https://polyhaven.com/a/%s | CC0 | %s |\n' \
            "$ASSET" "$ASSET" "$(date +%Y-%m-%d)" >> "$ASSETS/CREDITS.md"
        echo "  aggiunta la riga in assets/CREDITS.md"
    fi

    echo
    echo "fatto. Per tornare alla torre del kit:"
    echo "  rm -r assets/models/fort"
    exit 0
fi
```

- [ ] **Step 3: eseguirlo e verificare quello che è arrivato**

```bash
./tools/fetch_assets.sh forte
ls -la assets/models/fort assets/models/fort/textures
```

Atteso: `modular_fort_01.gltf` (~0,07 MB), `modular_fort_01.bin` (~1,29 MB) e
**nove** `.jpg` in `textures/` (~9,29 MB in tutto). Se i jpg sono meno di nove,
il download è incompleto e va rifatto: il `.gltf` li nomina tutti.

- [ ] **Step 4: verificare la riga dei crediti**

```bash
grep fort assets/CREDITS.md
```

Atteso: una riga con `assets/models/fort/modular_fort_01.gltf`, `Poly Haven`,
`CC0`.

- [ ] **Step 5: commit**

`assets/` non è versionato — si committa solo lo script.

```bash
git add tools/fetch_assets.sh
git commit -m "Scaricare un kit modulare intero, non un pezzo per file"
```

---

### Task 2: Scegliere un pezzo per indice, e accorgersi se è quello sbagliato

**Files:**
- Modify: `src/meshgroup.h` (in coda, prima di `#endif`)
- Modify: `src/meshgroup.c` (in coda)
- Create: `tools/prove/mastio.c`
- Modify: `Makefile:124-164` (una riga di collegamento nel bersaglio `prove`)

**Interfaces:**
- Consumes: `MeshGroupSplit()`, `MeshGroup` — già esistenti.
- Produces:
  - `const MeshGroup *MeshGroupPick(const MeshGroup *g, int ng, int pezzo)`
    — `NULL` se l'indice non esiste.
  - `bool MeshGroupSomiglia(const MeshGroup *g, Vector3 atteso, float tolleranza)`
    — tolleranza **relativa, per lato**.

- [ ] **Step 1: scrivere la prova che fallisce**

Crea `tools/prove/mastio.c`:

```c
/* ============================================================================
 * mastio.c - Un kit modulare arriva in UN file: modular_fort_01 spedisce venti
 * pezzi di fortezza in 45 primitive. Il gioco ne usa uno solo, indirizzato per
 * INDICE, e questa prova fissa le due cose che possono andare storte in
 * silenzio:
 *
 *   1. l'indice pesca il pezzo sbagliato perche' l'ordinamento non e' totale;
 *   2. l'indice pesca un pezzo diverso perche' il catalogo ha ricotto il file,
 *      e nessuno se ne accorge finche' non si guarda la torre.
 *
 * Niente OpenGL: e' geometria pura, e gira anche dove le prove di rendering
 * escono 77.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "prova.h"

/* Ingombri ASIMMETRICI: un ingombro cubico non si accorge se si scambia
 * l'altezza con la profondita', ed e' lo stesso inganno del cubo nella prova
 * dell'instancing. */
static BoundingBox Box(float x0, float z0, float lx, float ly, float lz)
{
    BoundingBox b;
    b.min = (Vector3){ x0,      0.0f, z0      };
    b.max = (Vector3){ x0 + lx, ly,   z0 + lz };
    return b;
}

int main(void)
{
    /* Quattro pezzi copiati dalle misure vere di modular_fort_01. I primi due
     * cominciano ALLA STESSA X - e' il pareggio che il file contiene davvero,
     * due volte - e li separa solo la Z. */
    BoundingBox pezzi[4] = {
        Box(10.00f, -34.00f,  4.17f,  8.53f, 14.56f),  /* wall_thick_straight_01 */
        Box(10.00f, -14.00f,  2.55f,  8.61f, 14.82f),  /* wall_thin_straight_02  */
        Box(19.00f, -34.00f,  3.12f,  7.53f, 14.56f),  /* wall_walkway_straight  */
        Box(32.20f, -33.95f, 15.84f, 13.50f, 15.84f),  /* tower_round            */
    };
    int idx[4];
    MeshGroup gr[4];
    int ng = MeshGroupSplit(pezzi, 4, idx, gr, 4);
    Ok("quattro pezzi separati -> quattro gruppi", ng == 4);

    /* Il pareggio su min.x si risolve sulla min.z. Senza questo controllo il
     * sabotaggio che toglie il secondo criterio passerebbe: la torre resterebbe
     * comunque in fondo, e solo i primi due si scambierebbero. */
    Ok("il pareggio su min.x lo risolve la min.z (gruppo 0)",
       ng == 4 && fabsf((gr[0].box.max.z - gr[0].box.min.z) - 14.56f) < 0.01f);
    Ok("il pareggio su min.x lo risolve la min.z (gruppo 1)",
       ng == 4 && fabsf((gr[1].box.max.z - gr[1].box.min.z) - 14.82f) < 0.01f);

    /* La selezione per indice: nel file vero la torre e' il gruppo 12, qui il
     * 3. Si riconosce dalla taglia, non dalla posizione nell'array. */
    const MeshGroup *torre = MeshGroupPick(gr, ng, 3);
    Ok("l'indice pesca il pezzo giusto",
       torre != NULL && fabsf((torre->box.max.y - torre->box.min.y) - 13.50f) < 0.01f);

    /* Un indice che non esiste NON e' un errore da ignorare: un asset ricotto
     * puo' avere meno pezzi di quando fu misurato, e il chiamante deve poter
     * ripiegare. */
    Ok("un indice oltre i gruppi torna NULL",  MeshGroupPick(gr, ng, 4)  == NULL);
    Ok("un indice negativo torna NULL",        MeshGroupPick(gr, ng, -1) == NULL);

    /* La guardia contro l'asset ricotto. */
    Vector3 atteso = { 15.84f, 13.50f, 15.84f };
    Ok("la torre somiglia a quella dichiarata",
       MeshGroupSomiglia(torre, atteso, 0.20f));
    Ok("un bastione NON somiglia alla torre",
       !MeshGroupSomiglia(MeshGroupPick(gr, ng, 0), atteso, 0.20f));

    /* Lo scarto si misura per LATO, non sul volume: un pezzo che sbaglia una
     * sola dimensione deve cadere lo stesso. Qui altezza e profondita' sono
     * giuste e solo la larghezza e' meta'. */
    BoundingBox stortoBox = Box(0.0f, 0.0f, 7.92f, 13.50f, 15.84f);
    MeshGroup storto = { 0, 1, stortoBox };
    Ok("un pezzo giusto su due lati e sbagliato sul terzo cade",
       !MeshGroupSomiglia(&storto, atteso, 0.20f));

    /* Dentro tolleranza si accetta: una ricottura che cambia l'asset di poco
     * non deve spegnere la torre. */
    BoundingBox vicinoBox = Box(0.0f, 0.0f, 16.60f, 14.10f, 16.60f);
    MeshGroup vicino = { 0, 1, vicinoBox };
    Ok("uno scarto del 5% resta dentro tolleranza",
       MeshGroupSomiglia(&vicino, atteso, 0.20f));

    return ProveEsito();
}
```

- [ ] **Step 2: aggiungere la riga al Makefile**

In `Makefile`, nel bersaglio `prove`, dopo il blocco di `varianti`:

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/mastio.c \
	      $(SRC_DIR)/fmath.c $(SRC_DIR)/light.c $(SRC_DIR)/instancing.c \
	      $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c \
	      $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/mastio
```

`meshgroup.c` **non** si collega: `mastio.c` lo include, e collegarlo anche
duplicherebbe i simboli. È la stessa riga di `varianti`.

- [ ] **Step 3: eseguire la prova e verificare che NON compili**

```bash
make prove
```

Atteso: errore di compilazione, `MeshGroupPick` e `MeshGroupSomiglia` non
dichiarate. È il fallimento giusto: la prova esiste prima della funzione.

- [ ] **Step 4: dichiarare le due funzioni**

In `src/meshgroup.h`, prima di `#endif /* MESHGROUP_H */`:

```c
/* Quale gruppo tocca a un pezzo dichiarato per INDICE. Torna NULL se l'indice
 * non esiste.
 *
 * L'indice e' RIPRODUCIBILE - MeshGroupSplit ordina per min.x e, a parita', per
 * min.z - ma non e' STABILE NEL TEMPO: se il catalogo ricuoce il file e
 * riordina i pezzi, lo stesso indice pesca un altro oggetto. Un indice fuori
 * dai gruppi non e' quindi un errore da ignorare: e' un ripiego. */
const MeshGroup *MeshGroupPick(const MeshGroup *g, int ng, int pezzo);

/* L'ingombro del gruppo somiglia a quello dichiarato? 'tolleranza' e' RELATIVA
 * e vale PER LATO: 0,2 accetta un quinto di scarto su ognuno dei tre.
 *
 * E' l'altra meta' della difesa contro un asset ricotto: l'indice dice dove
 * guardare, questa dice se cio' che si e' trovato e' ancora quella cosa. Per
 * lato e non sul volume, o un pezzo largo il doppio e alto la meta' passerebbe. */
bool MeshGroupSomiglia(const MeshGroup *g, Vector3 atteso, float tolleranza);
```

- [ ] **Step 5: scrivere le due funzioni**

In coda a `src/meshgroup.c`:

```c
const MeshGroup *MeshGroupPick(const MeshGroup *g, int ng, int pezzo)
{
    if (g == NULL || pezzo < 0 || pezzo >= ng) return NULL;
    return &g[pezzo];
}

bool MeshGroupSomiglia(const MeshGroup *g, Vector3 atteso, float tolleranza)
{
    if (g == NULL || tolleranza < 0.0f) return false;

    float lato[3] = { g->box.max.x - g->box.min.x,
                      g->box.max.y - g->box.min.y,
                      g->box.max.z - g->box.min.z };
    float att[3]  = { atteso.x, atteso.y, atteso.z };

    for (int i = 0; i < 3; i++) {
        /* Un lato atteso nullo non e' una dichiarazione, e' una riga vuota:
         * accettarlo vorrebbe dire accettare qualunque cosa. */
        if (att[i] <= 1e-4f) return false;
        if (fabsf(lato[i] - att[i]) > att[i] * tolleranza) return false;
    }
    return true;
}
```

- [ ] **Step 6: eseguire le prove**

```bash
make prove
```

Atteso: `== build/prove/mastio` con dieci righe `ok` e `tutto a posto`, e tutte
le altre prove come prima.

- [ ] **Step 7: sabotare, uno per volta, rimettendo a posto ogni volta**

Ogni sabotaggio deve far **fallire** `make prove`. Se passa, la prova non prova
quello che dice e va riscritta.

1. In `PrimaDi()` (`src/meshgroup.c`), togliere il criterio sulla `min.z`:
   `return a.min.x < b.min.x;` — devono cadere le due righe sul pareggio.
2. In `MeshGroupPick()`, cambiare `&g[pezzo]` in `&g[pezzo > 0 ? pezzo - 1 : 0]`
   — deve cadere "l'indice pesca il pezzo giusto".
3. In `MeshGroupSomiglia()`, sostituire il confronto per lato con uno sul
   volume:
   `float v = lato[0]*lato[1]*lato[2], a = att[0]*att[1]*att[2]; return fabsf(v-a) <= a*tolleranza;`
   — deve cadere "un pezzo giusto su due lati e sbagliato sul terzo cade".
4. In `MeshGroupSomiglia()`, portare la tolleranza a `tolleranza + 1.0f` — deve
   cadere "un bastione NON somiglia alla torre".

- [ ] **Step 8: commit**

```bash
git add src/meshgroup.h src/meshgroup.c tools/prove/mastio.c Makefile
git commit -m "Scegliere un pezzo per indice, e accorgersi se non e' quello"
```

---

### Task 3: Ricentrare solo il pezzo scelto

**Files:**
- Modify: `src/world.c` (una funzione nuova, accanto a `LoadExtProps()` — cioè
  prima di `LoadBuildParts()`)
- Modify: `tools/prove/mastio.c`

**Interfaces:**
- Consumes: `MeshGroupRecenter()`, `MeshGroupOrigin()`, `MeshGroup`.
- Produces:
  `static int RicentraPezzo(Model *m, const int *idx, const MeshGroup *g, Vector3 o)`
  — sposta i vertici sulla **CPU** e torna quante mesh ha spostato. Il Task 5
  la chiama e poi fa `UpdateMeshBuffer()` sulle stesse mesh.

- [ ] **Step 1: scrivere la prova che fallisce**

In `tools/prove/mastio.c`, prima di `return ProveEsito();`:

```c
    /* --- Il ricentraggio tocca SOLO il pezzo scelto ----------------------
     * raylib fonde le trasformazioni dei nodi dentro i vertici, quindi ogni
     * pezzo porta cucito l'offset che lo mette in fila con gli altri
     * diciannove. Va tolto - ma solo al pezzo che si disegna: spostare anche
     * gli altri non si vedrebbe mai, perche' nessuno li disegna, e resterebbe
     * li' finche' qualcuno non ne usa un secondo.
     *
     * Model e Mesh sono strutture semplici: se ne costruisce una a mano, senza
     * LoadModel e quindi senza contesto grafico. RicentraPezzo non tocca la
     * scheda apposta. */
    float v0[6] = { 10.0f, 0.0f, -34.0f,  14.0f, 8.0f, -20.0f };
    float v1[6] = { 32.2f, 0.0f, -33.9f,  48.0f, 13.5f, -18.1f };
    float v2[6] = { 19.0f, 0.0f, -34.0f,  22.0f, 7.5f, -20.0f };
    float atteso0[6], atteso2[6];
    for (int i = 0; i < 6; i++) { atteso0[i] = v0[i]; atteso2[i] = v2[i]; }

    Mesh mesh[3];
    for (int i = 0; i < 3; i++) { mesh[i] = (Mesh){ 0 }; mesh[i].vertexCount = 2; }
    mesh[0].vertices = v0;
    mesh[1].vertices = v1;
    mesh[2].vertices = v2;

    Model fake = { 0 };
    fake.meshCount = 3;
    fake.meshes    = mesh;

    /* Il gruppo scelto e' la sola mesh 1. 'first' indicizza midx, non le mesh:
     * e' la convenzione che MeshGroupSplit riempie. */
    int       midx[3] = { 0, 1, 2 };
    MeshGroup scelto  = { 1, 1, Box(32.2f, -33.9f, 15.84f, 13.50f, 15.84f) };

    Vector3 o = MeshGroupOrigin(&scelto);
    int mosse = RicentraPezzo(&fake, midx, &scelto, o);

    Ok("ricentra una mesh sola", mosse == 1);
    Ok("il pezzo scelto va sull'origine",
       fabsf(v1[0] - (32.2f - o.x)) < 0.01f &&
       fabsf(v1[1] - (0.0f  - o.y)) < 0.01f &&
       fabsf(v1[2] - (-33.9f - o.z)) < 0.01f);

    int fermi = 1;
    for (int i = 0; i < 6; i++)
        if (v0[i] != atteso0[i] || v2[i] != atteso2[i]) fermi = 0;
    Ok("gli altri pezzi non si muovono", fermi);

    /* Una mesh senza vertici non si conta e non si deferenzia: dopo LoadModel
     * non capita, ma chi chiama fa UpdateMeshBuffer() sulle mesh mosse, e
     * quella funzione deferenzia anche vboId. */
    mesh[1].vertices = NULL;
    Ok("una mesh senza vertici non si conta",
       RicentraPezzo(&fake, midx, &scelto, o) == 0);
```

- [ ] **Step 2: eseguire e verificare che NON compili**

```bash
make prove
```

Atteso: errore, `RicentraPezzo` non dichiarata.

- [ ] **Step 3: scrivere la funzione**

In `src/world.c`, subito prima di `LoadBuildParts()`:

```c
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
```

- [ ] **Step 4: eseguire le prove**

```bash
make prove
```

Atteso: le quattro righe nuove `ok`, e `make` senza avvisi.

- [ ] **Step 5: sabotare**

1. In `RicentraPezzo()`, ciclare su tutte le mesh del modello invece che sul
   gruppo: `for (int k = 0; k < m->meshCount; k++) { Mesh *me = &m->meshes[k]; ... }`
   — deve cadere "gli altri pezzi non si muovono" **e** "ricentra una mesh sola".
2. Togliere il `continue` sulla mesh senza vertici — deve cadere "una mesh senza
   vertici non si conta".
3. In `MeshGroupOrigin()` (`src/meshgroup.c`), usare il centro in Y invece del
   minimo — deve cadere "il pezzo scelto va sull'origine". È il controllo che
   tiene la torre appoggiata a terra e non mezza sottoterra.

- [ ] **Step 6: commit**

```bash
git add src/world.c tools/prove/mastio.c
git commit -m "Il ricentraggio e' del pezzo scelto, non di tutto il file"
```

---

### Task 4: La riga che dice anche *quale pezzo*

Questo compito non cambia **nessun comportamento**: prepara le tabelle e i campi
perché il Task 5 li riempia. Il gioco dopo questo compito è identico a prima.

**Files:**
- Modify: `src/worldtypes.h:27-31` (l'enum `BuildPart`)
- Modify: `src/world.c:191-235` (le tabelle `BUILD_FILES` e `gBuildMat`)
- Modify: `src/world.h:79-88` (i campi di `World`)

**Interfaces:**
- Produces: `BUILD_KEEP` in `BuildPart`; `BUILD_FILES[i].pezzo/.atteso/.voluto/
  .perAltezza`; `gBuildMat[i].uvVere`; i campi `World.buildLoaded/buildOwned/
  partIdx/partIdxN/hasKeep/keepScale/keepHalf/keepHigh`.

- [ ] **Step 1: aggiungere il pezzo all'enum, in fondo**

In `src/worldtypes.h`, sostituire l'enum `BuildPart` con:

```c
/* Pezzi con cui si costruiscono casa e torre. Nei kit CC0 gli edifici
 * medievali sono modulari - muro, muro con porta, falda, solaio - su una
 * griglia di celle da 1 unita': un edificio e' una ricetta, non un file.
 *
 * BUILD_KEEP sta ULTIMO e non e' come gli altri: e' un pezzo dentro un file di
 * venti, arriva da un altro catalogo, ha UV vere e non entra nel "tutti o
 * nessuno" dei pezzi dei kit. I dieci obbligatori sono quelli PRIMA di lui, e
 * si contano cosi' - senza una seconda costante da tenere allineata a mano. */
typedef enum {
    BUILD_WALL, BUILD_DOOR, BUILD_WINDOW, BUILD_ROOF, BUILD_FLOOR, BUILD_STAIRS,
    BUILD_TOWER_BASE, BUILD_TOWER_MID, BUILD_TOWER_TOP, BUILD_TOWER_ROOF,
    BUILD_KIT_COUNT,
    BUILD_KEEP = BUILD_KIT_COUNT,
    BUILD_PART_COUNT
} BuildPart;
```

`BUILD_KIT_COUNT` vale 10 e `BUILD_KEEP` vale 10 anche lui: sono lo stesso
indice visto da due parti, e `BUILD_PART_COUNT` diventa 11.

- [ ] **Step 2: sostituire la tabella dei file**

In `src/world.c`, al posto di `BUILD_FILES` (righe 191-206 circa):

```c
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
    int         pezzo;        /* -1 = tutto il file          */
    Vector3     atteso;       /* ingombro del pezzo, in metri */
    float       voluto;       /* taglia dichiarata, in metri  */
    bool        perAltezza;
} BUILD_FILES[BUILD_PART_COUNT] = {
    [BUILD_WALL]        = { "assets/models/town/wall.glb",                    -1, { 0 }, 0.0f, false },
    [BUILD_DOOR]        = { "assets/models/town/wall-doorway-round.glb",      -1, { 0 }, 0.0f, false },
    [BUILD_WINDOW]      = { "assets/models/town/wall-window-small.glb",       -1, { 0 }, 0.0f, false },
    [BUILD_ROOF]        = { "assets/models/town/roof-gable.glb",              -1, { 0 }, 0.0f, false },
    [BUILD_FLOOR]       = { "assets/models/town/planks.glb",                  -1, { 0 }, 0.0f, false },
    [BUILD_STAIRS]      = { "assets/models/town/stairs-wide-wood.glb",        -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_BASE]  = { "assets/models/castle/tower-square-base.glb",     -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_MID]   = { "assets/models/castle/tower-square-mid-windows.glb", -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_TOP]   = { "assets/models/castle/tower-square-top.glb",      -1, { 0 }, 0.0f, false },
    [BUILD_TOWER_ROOF]  = { "assets/models/castle/tower-square-top-roof.glb", -1, { 0 }, 0.0f, false },

    /* tower_round di modular_fort_01, misurato il 2026-09-08: gruppo 12 dei
     * venti, 2 mesh, 4.544 vertici, base a Y = 0. La taglia dichiarata e'
     * l'altezza vera dell'asset, quindi la scala esce 1,00 - il numero e'
     * dichiarato lo stesso, e non sottinteso. */
    [BUILD_KEEP]        = { "assets/models/fort/modular_fort_01.gltf",        12,
                            { 15.84f, 13.50f, 15.84f }, 13.50f, true },
};

/* Quanto puo' scostarsi l'ingombro misurato da quello dichiarato, per lato.
 * Un quinto: largo abbastanza da reggere una ricottura che cambia l'asset di
 * poco, stretto abbastanza da distinguere la torre tonda da ogni altro pezzo
 * del file - il piu' vicino per altezza e' un bastione da 8,61 m, che sta il
 * 36% sotto. */
#define BUILD_TOLLERANZA  0.20f
```

- [ ] **Step 3: dare voce ai pezzi che hanno UV vere**

In `src/world.c`, sostituire il `typedef` di `BuildMat` e aggiungere la riga di
`BUILD_KEEP` a `gBuildMat`:

```c
/* ... (il commento esistente resta) ...
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
};
```

- [ ] **Step 4: i campi nuovi di `World`**

In `src/world.h`, sostituire il blocco dei pezzi modulari:

```c
    /* Pezzi modulari degli edifici: vedi BUILD_FILES in world.c. Casa e torre
     * non esistono come modello unico nei kit CC0, si compongono. */
    Model  buildPart[BUILD_PART_COUNT];
    bool   hasBuildParts;          /* i dieci pezzi dei kit ci sono tutti */
    /* Un gruppo per tipo di pezzo: una casa bassa costa 19 chiamate, una alta
     * 45, e i tipi di pezzo sono dieci. */
    InstModel partBatch[BUILD_PART_COUNT];
    /* Quali pezzi hanno ricevuto un materiale proiettato: gli altri restano
     * alla tavolozza del kit e al modo 0. */
    bool   buildProj[BUILD_PART_COUNT];

    /* Un pezzo puo' venire da un file tutto suo o essere UNO dei venti dentro
     * un file solo. Da qui due bandiere invece di una:
     *   buildLoaded - il pezzo ha un modello utilizzabile;
     *   buildOwned  - ...e ne POSSIEDE il Model, quindi lo scarica lui. Un
     *                 pezzo che riusa il file di un altro non deve scaricarlo,
     *                 o la seconda UnloadModel() colpisce VBO gia' liberati. */
    bool   buildLoaded[BUILD_PART_COUNT];
    bool   buildOwned[BUILD_PART_COUNT];

    /* Le mesh del pezzo scelto dentro il file, per il ripiego non instanziato:
     * senza, DrawModelEx disegnerebbe tutti e venti i pezzi in un mucchio.
     * NULL per i pezzi che sono un file intero. */
    int   *partIdx[BUILD_PART_COUNT];
    int    partIdxN[BUILD_PART_COUNT];

    /* Il mastio: c'e' o non c'e', e se c'e' porta i suoi numeri. keepHalf e
     * keepHigh sono in METRI e gia' scalati, e li leggono sia la spinta del
     * giocatore sia il taglio della camera - un numero, due usi. */
    bool   hasKeep;
    float  keepScale;
    float  keepHalf, keepHigh;
```

- [ ] **Step 5: compilare, e verificare che non sia cambiato niente**

```bash
make && make prove && make valida
```

Atteso: zero avvisi, prove passate. Il gioco disegna ancora la torre Kenney:
`LoadBuildParts()` non conosce ancora `BUILD_KEEP` e il suo file non esiste per
lui — il ciclo "tutti o nessuno" però ora arriva fino a 11 e **fallirebbe**
sull'undicesimo. Il Task 5 lo aggiusta; per non lasciare il repo rotto fra i due
compiti, in questo step cambia subito i due cicli di `LoadBuildParts()` che
dicono `i < BUILD_PART_COUNT` per il controllo di esistenza e il caricamento in
`i < BUILD_KIT_COUNT`:

```c
    for (int i = 0; i < BUILD_KIT_COUNT; i++)
        if (!FileExists(BUILD_FILES[i].file)) return;

    for (int i = 0; i < BUILD_KIT_COUNT; i++) {
```

e, dentro il ciclo, `LoadModel(BUILD_FILES[i])` diventa
`LoadModel(BUILD_FILES[i].file)`, e il ciclo di rilascio dopo un caricamento
fallito resta com'è. Il conto finale dei pezzi proiettati e il suo `TraceLog`
passano anch'essi a `BUILD_KIT_COUNT`.

Verifica a occhio, con il gioco avviato: la torre è ancora quella di prima.

- [ ] **Step 6: commit**

```bash
git add src/worldtypes.h src/world.c src/world.h
git commit -m "Una riga di BUILD_FILES puo' dire anche QUALE pezzo"
```

---

### Task 5: Caricare il pezzo indicizzato

**Files:**
- Modify: `src/world.c` — `LoadBuildParts()` e il rilascio in `WorldUnload()`
  (righe 697-703 circa)

**Interfaces:**
- Consumes: `MeshGroupSplit()`, `MeshGroupPick()`, `MeshGroupSomiglia()`,
  `MeshGroupOrigin()`, `RicentraPezzo()`, `InstModelCreateSubset()`.
- Produces: `w->hasKeep`, `w->keepScale`, `w->keepHalf`, `w->keepHigh`,
  `w->partIdx[BUILD_KEEP]`, `w->partIdxN[BUILD_KEEP]`, `w->partBatch[BUILD_KEEP]`.

- [ ] **Step 1: la cache dei file**

In `src/world.c`, prima di `LoadBuildParts()`:

```c
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
```

- [ ] **Step 2: preparare il pezzo scelto**

Subito dopo `CaricaPezzo()`:

```c
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
    w->partIdx[i]  = (int *)MemAlloc((unsigned int)(scelto.count * (int)sizeof(int)));
    if (w->partIdx[i] == NULL) { MemFree(idx); MemFree(gr); return false; }
    for (int k = 0; k < scelto.count; k++) w->partIdx[i][k] = idx[scelto.first + k];
    w->partIdxN[i] = scelto.count;

    /* I numeri della taglia sono del MASTIO, non di un pezzo indicizzato
     * qualunque: keepScale, keepHalf e keepHigh hanno un solo destinatario, e
     * il secondo pezzo indicizzato - la cripta della domanda E - vorra' i
     * propri. Scriverli qui senza guardia vorrebbe dire che il secondo
     * sovrascrive in silenzio la torre. */
    if (i == BUILD_KEEP) {
        w->keepScale = MeshGroupScale(&scelto, BUILD_FILES[i].voluto,
                                      BUILD_FILES[i].perAltezza);
        float lx = (scelto.box.max.x - scelto.box.min.x) * w->keepScale;
        float lz = (scelto.box.max.z - scelto.box.min.z) * w->keepScale;
        w->keepHalf = 0.5f * ((lx > lz) ? lx : lz);
        w->keepHigh = (scelto.box.max.y - scelto.box.min.y) * w->keepScale;
    }

    InstModelCreateSubset(&w->partBatch[i], *m, w->partIdx[i], w->partIdxN[i]);

    MemFree(idx); MemFree(gr);
    return true;
}
```

- [ ] **Step 3: chiamarle in coda a `LoadBuildParts()`**

In `src/world.c`, dentro `LoadBuildParts()`, subito prima della riga
`w->hasBuildParts = true;`, i dieci pezzi dei kit dichiarano di essere caricati:

```c
    for (int i = 0; i < BUILD_KIT_COUNT; i++) {
        w->buildLoaded[i] = true;
        w->buildOwned[i]  = true;
    }
```

E dopo il `TraceLog` finale del conto dei pezzi proiettati, in coda alla
funzione:

```c
    /* Il mastio del forte NON entra nel "tutti o nessuno" dei pezzi dei kit:
     * quel controllo esiste perche' mezza casa e' peggio di una scatola, mentre
     * una torre Kenney e' un edificio intero e giusto, solo stilizzato. Se il
     * forte non c'e', o e' un altro pezzo di quello atteso, la torre resta
     * quella di prima e non e' un errore. */
    if (CaricaPezzo(w, BUILD_KEEP)) {
        w->buildLoaded[BUILD_KEEP] = true;
        LightApplyToModel(&w->buildPart[BUILD_KEEP]);

        if (PreparaPezzo(w, BUILD_KEEP)) {
            w->hasKeep = true;
            TraceLog(LOG_INFO,
                     "WORLD: mastio %s pezzo %d (%d mesh, x%.2f -> %.1f m, "
                     "semiampiezza %.2f m)%s",
                     BUILD_FILES[BUILD_KEEP].file, BUILD_FILES[BUILD_KEEP].pezzo,
                     w->partIdxN[BUILD_KEEP], (double)w->keepScale,
                     (double)w->keepHigh, (double)w->keepHalf,
                     InstModelReady(&w->partBatch[BUILD_KEEP]) ? ", a lotti" : "");
        } else {
            /* Il modello resta caricato e verra' scaricato da WorldUnload:
             * buildLoaded lo dice. Quello che non c'e' e' il pezzo. */
            TraceLog(LOG_INFO, "WORLD: niente mastio, la torre resta quella del kit");
        }
    }
```

- [ ] **Step 4: aggiustare il rilascio**

In `src/world.c`, dentro `WorldUnload()`, sostituire il blocco
`if (w->hasBuildParts) { ... }`:

```c
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
```

- [ ] **Step 5: compilare e leggere il registro**

```bash
make && make prove
```

Poi, con il gioco avviato — il registro su pipe è bufferizzato, quindi va
rediretto su file o si legge una schermata vuota:

```bash
./frostmark > /tmp/frostmark.log 2>&1
grep -i "mastio\|pezzi per gli edifici" /tmp/frostmark.log
```

Atteso, con `assets/models/fort/` presente:

```
WORLD: mastio assets/models/fort/modular_fort_01.gltf pezzo 12 (2 mesh, x1.00 -> 13.5 m, semiampiezza 7.92 m), a lotti
```

`x1.00` conferma che la taglia dichiarata è quella vera dell'asset, e `7.92`
che la semiampiezza sta sotto gli 8 m che l'anziano impone.

- [ ] **Step 6: verificare il ripiego, che è metà del lavoro**

```bash
mv assets/models/fort /tmp/fort-via
./frostmark > /tmp/senza.log 2>&1
grep -i "mastio" /tmp/senza.log
mv /tmp/fort-via assets/models/fort
```

Atteso: **nessuna riga** sul mastio, nessun avviso, e la torre Kenney in gioco
come prima. Poi la guardia: cambia a mano `BUILD_FILES[BUILD_KEEP].pezzo` da
`12` a `0`, ricompila e avvia — deve comparire la riga
`pezzo 0 misura 4.17x8.53x14.56, atteso 15.84x13.50x15.84: si ripiega`, e la
torre deve restare quella del kit. Rimetti `12`.

- [ ] **Step 7: commit**

```bash
git add src/world.c
git commit -m "Caricare il pezzo dodici di venti, e ripiegare se non e' lui"
```

---

### Task 6: Disegnare il mastio

**Files:**
- Modify: `src/world.c` — `PlacePart()` (riga 823 circa), `DrawTower()` (947),
  il `case PROP_TOWER` di `DrawProp()` (1074), e i cicli dei lotti (1111, 1124)

**Interfaces:**
- Consumes: `w->hasKeep`, `w->keepScale`, `w->partIdx[BUILD_KEEP]`,
  `w->partBatch[BUILD_KEEP]`.
- Produces: `static void DrawKeep(World *w, Vector3 pos, float rotDeg, float s, Color tint)`.

- [ ] **Step 1: il ripiego non instanziato deve disegnare solo il pezzo**

In `PlacePart()`, sostituire il ramo `else` (quello senza lotto):

```c
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
                 * davvero, ma riparte ogni fotogramma dallo stesso WHITE e non
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
```

- [ ] **Step 2: la ricetta del mastio**

In `src/world.c`, subito dopo `DrawTower()`:

```c
/* Mastio: un pezzo solo, appoggiato a terra.
 *
 * Non usa la griglia delle celle - BUILD_CELL vale 2,6 m e regge le case, non
 * una torre da 15,84 - quindi la scala e' metrica e arriva dall'ingombro
 * misurato al caricamento. Con lx = ly = lz = 0 la cella non entra nel conto,
 * e si passa 1 per dirlo. */
static void DrawKeep(World *w, Vector3 pos, float rotDeg, float s, Color tint)
{
    float k = w->keepScale * s;
    PlacePart(w, BUILD_KEEP, pos, rotDeg, 0.0f, 0.0f, 0.0f, 0.0f,
              1.0f, (Vector3){ k, k, k }, tint);
}
```

- [ ] **Step 3: sceglierlo al posto della torre del kit**

In `DrawProp()`, il `case PROP_TOWER`:

```c
        case PROP_TOWER: {
            if (w->hasKeep)       { DrawKeep(w, pos, p->rot, s, Shade(WHITE, tint)); break; }
            if (w->hasBuildParts) { DrawTower(w, pos, p->rot, s, Shade(WHITE, tint)); break; }
            Vector3 body = pos;
            DrawModelEx(w->mCyl, body, Y, 0.0f, (Vector3){3.0f, 11.0f, 3.0f},
                        Shade((Color){ 138, 134, 128, 255 }, tint));
            Vector3 roof = { pos.x, pos.y + 11.0f, pos.z };
            DrawModelEx(w->mCone, roof, Y, 45.0f, (Vector3){3.6f, 3.4f, 3.6f},
                        Shade((Color){ 92, 58, 46, 255 }, tint));
        } break;
```

Il ripiego resta a due livelli: il mastio se c'è, i pezzi Kenney se no, il
cilindro procedurale se non c'è nemmeno quello.

- [ ] **Step 4: verificare che i cicli dei lotti coprano il pezzo nuovo**

I due cicli in `DrawProp`/`WorldDraw` (righe 1111 e 1124 circa) girano già su
`BUILD_PART_COUNT`, che ora vale 11: `InstModelBegin()` e `InstModelFlush()`
prendono `BUILD_KEEP` da soli, e `InstModelReady()` è falso quando il lotto non
c'è. Leggerli e confermare — non serve modificarli. Il `BUILD_ROOF` resta
l'unico escluso dallo svuotamento generale, e questo non cambia.

- [ ] **Step 5: compilare e guardare**

```bash
make && make prove
./frostmark
```

Atteso a occhio: al centro del villaggio c'è una torre tonda in pietra, molto
più larga delle case attorno, appoggiata a terra e non sprofondata né sospesa.
Se è sprofondata, `MeshGroupOrigin()` non sta usando il minimo in Y; se è in
mezzo a diciannove muri, `w->partIdx` non è arrivato al ripiego.

- [ ] **Step 6: commit**

```bash
git add src/world.c
git commit -m "Al centro del villaggio una torre tonda, non quattro cubi"
```

---

### Task 7: La collisione segue la torre, da un numero solo

**Files:**
- Modify: `src/world.c` — `WorldCameraClip()` (riga 1436 circa),
  `WorldResolveCollision()` (1590 circa), più una funzione nuova
- Modify: `tools/prove/mastio.c`

**Interfaces:**
- Produces: `static float TowerHalf(const World *w, const Prop *p)` e
  `static float TowerHigh(const World *w, const Prop *p)` — semiampiezza e
  altezza in metri, già moltiplicate per `p->scale`.

- [ ] **Step 1: scrivere la prova che fallisce**

In `tools/prove/mastio.c`, prima di `return ProveEsito();`:

```c
    /* --- Un numero, due usi ----------------------------------------------
     * La spinta del giocatore e il taglio della camera devono leggere la
     * STESSA taglia. Se divergessero, la camera entrerebbe in un muro che il
     * giocatore non puo' attraversare - ed e' lo stesso motivo per cui
     * StairTop() serve sia a chi cammina sulla rampa sia a chi ci sbatte
     * contro. */
    /* World e' grosso: static, o si rischia la pila. */
    static World mondo;
    Prop  torreProp = { 0 };
    torreProp.type   = PROP_TOWER;
    torreProp.scale  = 1.0f;
    torreProp.radius = 3.0f;          /* il raggio cotto nel mondo */

    /* Le due funzioni valgono per il MASTIO: i due chiamanti le invocano solo
     * dentro 'if (hasKeep)', e senza mastio i numeri di prima - 3,0 cotto per
     * la spinta, 1,6 e 12,0 per la scatola della camera - restano scritti dove
     * sono sempre stati. Un valore di ripiego qui dentro sarebbe codice che
     * non legge nessuno, e una prova che lo controlla proverebbe il nulla. */
    mondo.hasKeep  = true;
    mondo.keepHalf = 7.92f;
    mondo.keepHigh = 13.50f;
    Ok("la spinta usa la semiampiezza misurata",
       fabsf(TowerHalf(&mondo, &torreProp) - 7.92f) < 0.01f);
    Ok("l'altezza e' quella misurata, non la semiampiezza",
       fabsf(TowerHigh(&mondo, &torreProp) - 13.50f) < 0.01f);

    /* La scala dell'istanza vale su entrambi, o una torre rimpicciolita
     * spingerebbe alla taglia di quella intera. */
    torreProp.scale = 0.5f;
    Ok("la scala dell'istanza vale sulla semiampiezza",
       fabsf(TowerHalf(&mondo, &torreProp) - 3.96f) < 0.01f);
    Ok("la scala dell'istanza vale sull'altezza",
       fabsf(TowerHigh(&mondo, &torreProp) - 6.75f) < 0.01f);

    /* Il tetto che fissa la taglia: l'anziano nasce a 14 m dal centro del
     * villaggio, ed e' cotto nel mondo come i prop. Una semiampiezza oltre gli
     * 8 m lo farebbe nascere dentro la pietra. */
    torreProp.scale = 1.0f;
    Ok("la semiampiezza sta sotto gli 8 m che l'anziano impone",
       TowerHalf(&mondo, &torreProp) < 8.0f);
```

- [ ] **Step 2: eseguire e verificare che NON compili**

```bash
make prove
```

Atteso: errore, `TowerHalf` e `TowerHigh` non dichiarate.

- [ ] **Step 3: scrivere le due funzioni**

In `src/world.c`, prima di `WorldCameraClip()` — cioè prima del primo dei due
usi, così chi legge le incontra una volta sola:

```c
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
```

- [ ] **Step 4: la camera taglia su un cilindro**

In `WorldCameraClip()`, sostituire il blocco di torre e cripta:

```c
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
```

La scatola della torre del kit resta a 1,6 e 12,0 come oggi: senza mastio nulla
cambia, ed è la promessa del ripiego.

- [ ] **Step 5: la spinta del giocatore**

In `WorldResolveCollision()`, subito dopo il blocco della casa e prima di
`if (p->radius <= 0.0f) continue;`:

```c
            /* La torre, quando e' il mastio, e' larga cinque volte il raggio
             * cotto nel mondo: lo si scavalca, con la stessa taglia che usa la
             * camera. */
            if (p->type == PROP_TOWER && w->hasKeep) {
                float dx = pos->x - p->pos.x, dz = pos->z - p->pos.z;
                float d2 = dx * dx + dz * dz;
                float rr = TowerHalf(w, p) + radius;
                if (d2 < rr * rr && d2 > 0.0001f) {
                    float d = sqrtf(d2);
                    float push = (rr - d) / d;
                    pos->x += dx * push;
                    pos->z += dz * push;
                }
                continue;
            }
```

- [ ] **Step 6: eseguire le prove**

```bash
make && make prove
```

Atteso: le sette righe nuove `ok`, zero avvisi.

- [ ] **Step 7: sabotare**

1. In `TowerHalf()`, togliere `* p->scale` — devono cadere le due righe sulla
   scala dell'istanza.
2. In `TowerHalf()`, tornare `p->radius * p->scale` (cioè il raggio cotto, che
   è il numero sbagliato ma plausibile) — deve cadere "la spinta usa la
   semiampiezza misurata".
3. In `TowerHigh()`, tornare `w->keepHalf` invece di `w->keepHigh` — deve cadere
   "l'altezza e' quella misurata, non la semiampiezza". È lo scambio fra due
   numeri plausibili, che a occhio non si vede.
4. Far leggere alla camera un numero suo — in `WorldCameraClip()`, `1.6f` al
   posto di `TowerHalf(w, p)`: le prove **non** lo prendono, ed è previsto. Si
   vede solo giocando, ed è il motivo per cui le due strade leggono la stessa
   funzione. Verifica a mano: con il sabotaggio la camera attraversa la torre;
   senza, no.

- [ ] **Step 8: verificare in gioco**

```bash
./frostmark
```

Cammina attorno alla torre: non ci si deve poter entrare da nessun lato, e la
camera non deve mai finire dentro la pietra. Poi togli `assets/models/fort/` e
rifai la stessa prova sulla torre Kenney: deve comportarsi come sempre.

- [ ] **Step 9: commit**

```bash
git add src/world.c tools/prove/mastio.c
git commit -m "La camera e il giocatore leggono la stessa taglia della torre"
```

---

### Task 8: Scrivere quello che si è imparato

**Files:**
- Modify: `docs/01-architettura.md` (sezione nuova, dopo *Varianti*)
- Modify: `docs/03-asset-pubblici.md` (sezione *Casa e torre*, riga 308 circa, e
  la tabella degli asset misurati alla riga 100 circa)
- Modify: `docs/06-stato-e-prossimi-passi.md` (la tabella dei cinque pezzi, la
  domanda D, il conto delle prove)

- [ ] **Step 1: `docs/01`, come funziona**

Dopo la sezione *Varianti*, una sezione **Pezzi indicizzati** che dice:

- un `BUILD_FILES` può nominare un pezzo dentro un file, per indice di gruppo;
- l'indice è riproducibile (`min.x`, poi `min.z`) ma non stabile nel tempo,
  quindi la riga dichiara anche l'ingombro atteso e si ripiega se non torna;
- il pezzo si ricentra come una variante, e il ripiego non instanziato disegna
  **solo** le sue mesh;
- la taglia è metrica, non in celle;
- `hasKeep` è indipendente da `hasBuildParts`, e perché.

- [ ] **Step 2: `docs/03`, come si sceglie**

Nella sezione *Casa e torre: un edificio è una ricetta, non un file*, aggiungere
che la torre ora è **un pezzo solo** preso da un kit modulare fotogrammetrico, e
la tabella dei pezzi va aggiornata da 5 a 1 per la torre.

Aggiungere `modular_fort_01` alla tabella degli asset misurati:

| asset Poly Haven | triangoli | vertici/mesh | alfa | si usa? |
|---|---|---|---|---|
| `modular_fort_01` | 28.218 | 4.148 | opaco | **sì — venti pezzi in un file, se ne usa uno** |

E una riga sul fatto che un **kit modulare** è il caso in cui il set di varianti
smette di essere un ostacolo e diventa un catalogo: si sceglie per indice.

- [ ] **Step 3: `docs/06`, dove siamo**

- nella tabella dei cinque pezzi, la torre passa da *stilizzata* a **fatto**;
- la domanda **D** si chiude **per la sola torre**, e resta aperta per mura,
  porta e cortile: dirlo, con il rimando alla spec;
- *Cosa è realistico in gioco, oggi* prende la riga del mastio con i suoi numeri;
- il conto delle prove passa da **sette** a **otto**, e `mastio` entra
  nell'elenco di quelle che girano senza contesto OpenGL — che diventano tre:
  `scale`, `varianti`, `mastio`;
- nella sezione dei sabotaggi, aggiungere quelli che hanno morso qui.

- [ ] **Step 4: verificare che il documento non menta**

```bash
make && make prove && make valida
ls tools/prove/*.c | wc -l
```

Atteso: otto file, prove passate, e ogni numero scritto nei documenti
ricontrollato contro il registro del gioco — non contro questo piano.

- [ ] **Step 5: commit**

```bash
git add docs/
git commit -m "I documenti dicono che la torre e' vera, e quanto costa"
```

---

## Note per chi esegue

**L'ordine conta.** I task 2 e 3 aggiungono funzioni provate senza toccare il
gioco; il 4 prepara le tabelle senza cambiare comportamento; dal 5 al 7 il
comportamento cambia, un pezzo per volta, e ognuno si può guardare da solo.

**Il Task 4 lascia il repo compilante solo se si fa anche lo Step 5**: cambiare
`BUILD_PART_COUNT` senza fermare i cicli di `LoadBuildParts()` a
`BUILD_KIT_COUNT` fa fallire il controllo "tutti o nessuno" sull'undicesimo file
e spegne tutti gli edifici. È scritto nel task, ma è la cosa che si dimentica.

**Non si ricuoce il mondo.** Se ti viene voglia di cambiare il `radius` in
`tools/worldgen.c`, fermati: i mondi già salvati non si aggiornerebbero, ed è
esattamente il motivo per cui `TowerHalf()` esiste.
