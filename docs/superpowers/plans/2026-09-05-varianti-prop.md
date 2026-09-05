# Varianti dei prop — piano d'implementazione

> **Per chi esegue:** SOTTO-SKILL RICHIESTA — usare superpowers:subagent-driven-development (consigliata) o superpowers:executing-plans, un compito per volta. I passi hanno la casella `- [ ]` per tenere il conto.

**Obiettivo:** un asset che contiene N individui affiancati (`shrub_02` sono quattro cespugli in fila) ne mostra **uno solo** per prop, scelto dalla posizione, alla taglia dichiarata.

**Architettura:** le mesh di un modello si raggruppano per contatto degli ingombri XZ; ogni gruppo è una variante, viene portata all'origine spostandone i vertici una volta al caricamento, e riceve il proprio gruppo di lotti d'instancing. La variante di un prop è una funzione pura della posizione, come `HouseShapeOf()`, così disegno, ombra e collisione non possono divergere.

**Tecnologie:** C99, raylib 5.5 (vendored in `vendor/raylib`), OpenGL 3.3 via `rlgl`. Nessuna dipendenza nuova.

**Spec:** `docs/superpowers/specs/2026-09-05-varianti-prop-design.md`

## Vincoli globali

- **I sorgenti sono in ASCII.** I commenti sono in italiano ma senza lettere accentate: si scrive `cosi'`, `piu'`, `e'`. Vale per `src/`, `tools/` e i messaggi di commit. I documenti in `docs/` usano gli accenti veri.
- **Zero avvisi.** Si compila con `-std=gnu99 -Wall -Wextra`; `make` costruisce Linux **e** Windows (sotto WSL) e non deve stampare avvisi.
- `SRCS` nel Makefile è `$(wildcard src/*.c)`: un file nuovo in `src/` entra da solo in entrambi i binari. Le prove no, hanno la loro riga di collegamento.
- **Le prove includono il `.c` che provano** (`#include "../../src/world.c"`), perché ciò che vale la pena provare è `static`. I moduli inclusi non vanno anche collegati o i simboli si duplicano.
- Una prova che non trova un contesto OpenGL esce `PROVA_SALTATA` (77) e `make prove` la conta come saltata.
- **Ogni prova nuova va verificata sabotando il codice** prima di considerarla finita: si rompe di proposito la funzione provata, si controlla che la prova fallisca, si rimette a posto.
- Il tetto dei vertici resta: `TroppiVertici()` scarta i modelli con una mesh oltre 65.535 vertici. Questo piano non lo tocca.

---

### Task 1: Raggruppare le mesh per ingombro

**File:**
- Crea: `src/meshgroup.h`, `src/meshgroup.c`
- Crea: `tools/prove/varianti.c`
- Modifica: `Makefile:137-150` (la ricetta `prove`)

**Interfacce:**
- Consuma: niente.
- Produce: `MeshGroup { int first, count; BoundingBox box; }` e
  `int MeshGroupSplit(const BoundingBox *box, int n, int *idx, MeshGroup *out, int maxGroups)`.
  `idx` e `out` li alloca il chiamante, almeno `n` elementi ciascuno. Torna il numero di gruppi, `0` se non se ne può fare niente.

- [ ] **Passo 1: scrivere la prova che fallisce**

Crea `tools/prove/varianti.c`:

```c
/* ============================================================================
 * varianti.c - Meta' del catalogo vegetale Poly Haven non e' un oggetto: e' un
 * set di individui affiancati. shrub_02 sono quattro cespugli in fila su sei
 * metri, periwinkle_plant sei piante su 1,2. Il gioco deve mostrarne UNO.
 *
 * Questa prova fissa il confine fra "sei piante affiancate" e "una pianta in
 * sei pezzi": la prima e' un set, la seconda no, e dopo LoadModel() l'unica
 * differenza visibile e' che nel secondo caso gli ingombri si sovrappongono -
 * Mesh di raylib non porta il nome.
 *
 * Niente OpenGL qui: il raggruppamento e' geometria pura, e una prova che non
 * chiede un contesto grafico gira anche dove le altre escono 77.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "prova.h"

/* Ingombri deliberatamente ASIMMETRICI: un ingombro cubico non si accorge se
 * si scambia l'altezza con la larghezza, ed e' lo stesso inganno del cubo
 * nella prova dell'instancing. */
static BoundingBox Box(float cx, float cz, float lx, float ly, float lz)
{
    BoundingBox b;
    b.min = (Vector3){ cx - lx * 0.5f, 0.0f, cz - lz * 0.5f };
    b.max = (Vector3){ cx + lx * 0.5f, ly,   cz + lz * 0.5f };
    return b;
}

int main(void)
{
    /* Quattro cespugli in fila, come shrub_02: separati lungo X. */
    BoundingBox fila[4] = {
        Box(0.0f, 0.0f, 0.8f, 1.1f, 0.6f),
        Box(2.0f, 0.0f, 0.9f, 1.4f, 0.7f),
        Box(4.0f, 0.0f, 0.7f, 0.9f, 0.5f),
        Box(6.0f, 0.0f, 1.0f, 1.6f, 0.8f),
    };
    int idx[4];
    MeshGroup gr[4];
    int n = MeshGroupSplit(fila, 4, idx, gr, 4);
    Ok("quattro cespugli in fila -> quattro varianti", n == 4);
    Ok("una mesh per variante", n == 4 && gr[0].count == 1 && gr[3].count == 1);

    /* L'ordine e' per X crescente, e deve reggere anche se l'array arriva
     * mescolato: e' l'ordine che decide quale indice tocca a quale variante, e
     * lo stesso mondo salvato deve mostrare gli stessi cespugli. */
    BoundingBox mesc[4] = { fila[2], fila[0], fila[3], fila[1] };
    n = MeshGroupSplit(mesc, 4, idx, gr, 4);
    int ordinato = (n == 4);
    for (int i = 1; i < n; i++)
        if (gr[i].box.min.x < gr[i - 1].box.min.x) ordinato = 0;
    Ok("i gruppi escono ordinati per X, comunque arrivino", ordinato);

    /* Una pianta sola in sei pezzi, come nettle_plant: gli ingombri si
     * sovrappongono perche' sono i materiali dello stesso individuo. */
    BoundingBox pianta[6];
    for (int i = 0; i < 6; i++)
        pianta[i] = Box(0.05f * (float)i, 0.0f, 0.9f, 1.2f + 0.1f * (float)i, 0.7f);
    int idx6[6];
    MeshGroup gr6[6];
    n = MeshGroupSplit(pianta, 6, idx6, gr6, 6);
    Ok("sei mesh sovrapposte -> una variante sola", n == 1);
    Ok("la variante unica tiene tutte le mesh", n == 1 && gr6[0].count == 6);

    /* Transitivita': A tocca B, B tocca C, A non tocca C. Sono un gruppo. */
    BoundingBox cat[3] = {
        Box(0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
        Box(0.8f, 0.0f, 1.0f, 1.2f, 1.0f),
        Box(1.6f, 0.0f, 1.0f, 0.8f, 1.0f),
    };
    int idx3[3];
    MeshGroup gr3[3];
    n = MeshGroupSplit(cat, 3, idx3, gr3, 3);
    Ok("il contatto e' transitivo: A-B-C fanno un gruppo", n == 1);

    /* L'ingombro del gruppo copre tutte le sue mesh, o la scala uscirebbe
     * sbagliata proprio sui set. */
    n = MeshGroupSplit(pianta, 6, idx6, gr6, 6);
    Ok("l'ingombro del gruppo e' l'unione delle sue mesh",
       n == 1 && gr6[0].box.max.y > 1.69f && gr6[0].box.max.y < 1.71f);

    /* Ogni mesh finisce in un gruppo e in uno solo. */
    n = MeshGroupSplit(fila, 4, idx, gr, 4);
    int visto[4] = { 0, 0, 0, 0 }, tutte = (n == 4);
    for (int g = 0; g < n; g++)
        for (int k = 0; k < gr[g].count; k++) visto[idx[gr[g].first + k]]++;
    for (int i = 0; i < 4; i++) if (visto[i] != 1) tutte = 0;
    Ok("ogni mesh sta in un gruppo e in uno solo", tutte);

    return ProveEsito();
}
```

Aggiungi la riga di compilazione nella ricetta `prove` del `Makefile`, subito dopo quella di `alfa` (`Makefile:149-150`). Non collega nient'altro: `meshgroup.c` è incluso dalla prova e non dipende da altri moduli.

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/varianti.c \
	      $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/varianti
```

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: errore di compilazione, `src/meshgroup.c` non esiste.

- [ ] **Passo 3: scrivere `src/meshgroup.h`**

```c
/* ============================================================================
 * meshgroup.h - Quali mesh di un modello sono lo stesso individuo.
 *
 * Meta' del catalogo vegetale Poly Haven e' fatta di SET: shrub_02 sono
 * quattro cespugli diversi in fila su sei metri, periwinkle_plant sei piante
 * affiancate su 1,2. Caricati come un oggetto solo, dove va un cespuglio ne
 * compaiono quattro in miniatura.
 *
 * Mesh di raylib 5.5 non porta il nome, quindi dopo LoadModel() l'unica
 * informazione rimasta e' la geometria. La regola e' una sola:
 *
 *     due mesh sono lo stesso individuo se i loro ingombri XZ si toccano.
 *
 * Distingue "sei piante affiancate" da "una pianta in sei pezzi" senza sapere
 * niente delle due: nel secondo caso i materiali stanno uno dentro l'altro.
 *
 * Qui non c'e' OpenGL e non c'e' raylib oltre ai tipi: e' la parte che si puo'
 * provare senza un contesto grafico.
 * ========================================================================== */
#ifndef MESHGROUP_H
#define MESHGROUP_H

#include "raylib.h"
#include <stdbool.h>

/* Oltre questo numero di mesh il raggruppamento si arrende e il chiamante
 * tratta il modello come un individuo solo. Il modello piu' composto visto nel
 * catalogo, grass_medium_01, ne ha diciassette. */
#define MESHGROUP_MAX 64

/* Un individuo: 'first' e 'count' indicizzano l'array riempito da
 * MeshGroupSplit(), 'box' e' l'ingombro di tutte le sue mesh. */
typedef struct {
    int         first, count;
    BoundingBox box;
} MeshGroup;

/* Divide n ingombri in gruppi. Due mesh finiscono nello stesso gruppo se i
 * loro ingombri XZ si toccano, e la relazione e' transitiva.
 *
 * 'idx' e 'out' li alloca il chiamante, almeno n elementi ciascuno: idx esce
 * con gli indici delle mesh raggruppati, out con i gruppi ORDINATI PER X
 * crescente - l'ordine decide quale indice tocca a quale variante e deve
 * essere lo stesso a ogni esecuzione e su ogni piattaforma.
 *
 * Torna il numero di gruppi, 0 se n non e' valido o supera MESHGROUP_MAX. */
int MeshGroupSplit(const BoundingBox *box, int n, int *idx,
                   MeshGroup *out, int maxGroups);

#endif /* MESHGROUP_H */
```

- [ ] **Passo 4: scrivere `src/meshgroup.c`**

```c
#include "meshgroup.h"
#include <math.h>

/* Si TOCCANO, non "si sovrappongono": due ingombri che si sfiorano sono lo
 * stesso individuo. Il rischio e' dichiarato nello spec - due varianti
 * modellate a contatto finirebbero insieme - e si vedrebbe subito, perche'
 * comparirebbero sempre in coppia. */
static bool ToccaXZ(BoundingBox a, BoundingBox b)
{
    return a.min.x <= b.max.x && b.min.x <= a.max.x &&
           a.min.z <= b.max.z && b.min.z <= a.max.z;
}

static BoundingBox Unione(BoundingBox a, BoundingBox b)
{
    BoundingBox u;
    u.min.x = fminf(a.min.x, b.min.x);
    u.min.y = fminf(a.min.y, b.min.y);
    u.min.z = fminf(a.min.z, b.min.z);
    u.max.x = fmaxf(a.max.x, b.max.x);
    u.max.y = fmaxf(a.max.y, b.max.y);
    u.max.z = fmaxf(a.max.z, b.max.z);
    return u;
}

/* Insiemi disgiunti con compressione del percorso: il contatto e' transitivo,
 * e una catena di mesh che si sfiorano a due a due e' un individuo solo. */
static int Radice(int *p, int i)
{
    while (p[i] != i) { p[i] = p[p[i]]; i = p[i]; }
    return i;
}

/* Ordine deterministico: X crescente, e a parita' di X la Z. Senza il secondo
 * criterio due varianti allineate sulla stessa X potrebbero scambiarsi di
 * posto fra un compilatore e l'altro, e lo stesso mondo salvato mostrerebbe
 * cespugli diversi. */
static bool PrimaDi(BoundingBox a, BoundingBox b)
{
    if (a.min.x != b.min.x) return a.min.x < b.min.x;
    return a.min.z < b.min.z;
}

int MeshGroupSplit(const BoundingBox *box, int n, int *idx,
                   MeshGroup *out, int maxGroups)
{
    if (box == NULL || idx == NULL || out == NULL) return 0;
    if (n <= 0 || n > MESHGROUP_MAX || maxGroups <= 0) return 0;

    int padre[MESHGROUP_MAX];
    for (int i = 0; i < n; i++) padre[i] = i;

    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (ToccaXZ(box[i], box[j])) padre[Radice(padre, i)] = Radice(padre, j);

    /* Una radice per gruppo, nell'ordine in cui compare. */
    int gruppoDi[MESHGROUP_MAX];
    for (int i = 0; i < n; i++) gruppoDi[i] = -1;

    int ng = 0;
    for (int i = 0; i < n; i++) {
        int r = Radice(padre, i);
        if (gruppoDi[r] == -1) {
            if (ng >= maxGroups) return 0;   /* il chiamante torna a una variante */
            gruppoDi[r]  = ng;
            out[ng].box   = box[i];
            out[ng].first = 0;
            out[ng].count = 0;
            ng++;
        } else {
            out[gruppoDi[r]].box = Unione(out[gruppoDi[r]].box, box[i]);
        }
        out[gruppoDi[r]].count++;
    }

    /* Ordinamento per selezione: i gruppi sono pochi, e serve la permutazione
     * per rimappare le mesh, non solo l'array ordinato. */
    int ordine[MESHGROUP_MAX];
    for (int k = 0; k < ng; k++) ordine[k] = k;
    for (int a = 0; a < ng; a++)
        for (int b = a + 1; b < ng; b++)
            if (PrimaDi(out[ordine[b]].box, out[ordine[a]].box)) {
                int t = ordine[a]; ordine[a] = ordine[b]; ordine[b] = t;
            }

    MeshGroup ord[MESHGROUP_MAX];
    int posDi[MESHGROUP_MAX], scritti[MESHGROUP_MAX];
    int acc = 0;
    for (int k = 0; k < ng; k++) {
        ord[k]              = out[ordine[k]];
        posDi[ordine[k]]    = k;
        ord[k].first        = acc;
        acc                += ord[k].count;
        scritti[k]          = 0;
    }

    for (int i = 0; i < n; i++) {
        int g = posDi[gruppoDi[Radice(padre, i)]];
        idx[ord[g].first + scritti[g]++] = i;
    }

    for (int k = 0; k < ng; k++) out[k] = ord[k];
    return ng;
}
```

- [ ] **Passo 5: lanciare le prove e vederle passare**

Esegui: `make prove`
Atteso: `== build/prove/varianti` con tutte le righe `ok`, e le altre prove invariate (saltate se manca la GPU).

- [ ] **Passo 6: sabotare, per verificare che la prova prenda**

Una per volta, ricompilando e rilanciando `make prove` a ogni sabotaggio, poi rimettendo a posto:

1. in `ToccaXZ`, cambia `a.min.x <= b.max.x` in `a.min.x < b.max.x - 0.5f` → devono fallire i controlli sulla transitività e sulla pianta unica;
2. togli il secondo giro dell'ordinamento (il `for (b = a + 1 ...)`) → deve fallire "i gruppi escono ordinati per X";
3. in `Unione`, usa `fminf` al posto di `fmaxf` per `max.y` → deve fallire "l'ingombro del gruppo e' l'unione delle sue mesh".

Se un sabotaggio non fa fallire niente, la prova che doveva prenderlo è da riscrivere, non da tenere.

- [ ] **Passo 7: commit**

```bash
git add src/meshgroup.h src/meshgroup.c tools/prove/varianti.c Makefile
git commit -m "Riconoscere le varianti dentro un modello"
```

---

### Task 2: Portare la variante all'origine e alla taglia giusta

**File:**
- Modifica: `src/meshgroup.h`, `src/meshgroup.c`
- Modifica: `tools/prove/varianti.c`

**Interfacce:**
- Consuma: `MeshGroup` e `MeshGroupSplit()` dal Task 1.
- Produce:
  - `Vector3 MeshGroupOrigin(const MeshGroup *g)` — centro XZ, minimo Y;
  - `void MeshGroupRecenter(float *v, int vertexCount, Vector3 o)` — sposta i vertici di `-o`, `v` è l'array di raylib, 3 float per vertice;
  - `float MeshGroupScale(const MeshGroup *g, float voluto, bool perAltezza)` — moltiplicatore verso la dimensione voluta in metri.

- [ ] **Passo 1: scrivere le prove che falliscono**

Aggiungi in `tools/prove/varianti.c`, prima di `return ProveEsito();`:

```c
    /* --- Ricentrare -------------------------------------------------------
     * raylib fonde le trasformazioni dei nodi dentro i vertici, quindi la
     * seconda variante di shrub_02 porta cucito l'offset che la mette in fila.
     * Senza toglierlo, il cespuglio comparirebbe due metri di lato. */
    n = MeshGroupSplit(fila, 4, idx, gr, 4);
    Vector3 o = MeshGroupOrigin(&gr[1]);
    Ok("l'origine e' il centro XZ della variante",
       n == 4 && fabsf(o.x - 2.0f) < 1e-4f && fabsf(o.z) < 1e-4f);
    Ok("l'origine e' il MINIMO Y, non il centro: la pianta sta a terra",
       fabsf(o.y) < 1e-4f);

    /* Un triangolo dove sta la seconda variante: x attorno a 2, y da 0 a 1,4 */
    float v[9] = { 1.6f, 0.0f, -0.3f,
                   2.4f, 0.0f,  0.3f,
                   2.0f, 1.4f,  0.0f };
    MeshGroupRecenter(v, 3, o);
    Ok("dopo il ricentraggio la variante sta sull'origine",
       fabsf((v[0] + v[3] + v[6]) / 3.0f) < 1e-4f &&
       fabsf((v[2] + v[5] + v[8]) / 3.0f) < 1e-4f);
    Ok("il ricentraggio non schiaccia la variante a terra",
       fabsf(v[7] - 1.4f) < 1e-4f);

    /* --- La taglia --------------------------------------------------------
     * Ogni variante si scala dal PROPRIO ingombro fino alla dimensione
     * dichiarata: le varianti cambiano forma, non misura, e il raggio di
     * collisione cotto nel mondo resta valido. */
    n = MeshGroupSplit(fila, 4, idx, gr, 4);
    float k0 = MeshGroupScale(&gr[0], 1.4f, true);   /* alta 1,1 */
    float k3 = MeshGroupScale(&gr[3], 1.4f, true);   /* alta 1,6 */
    float h0 = (gr[0].box.max.y - gr[0].box.min.y) * k0;
    float h3 = (gr[3].box.max.y - gr[3].box.min.y) * k3;
    Ok("varianti di altezza diversa arrivano alla stessa taglia",
       fabsf(h0 - 1.4f) < 1e-4f && fabsf(h3 - 1.4f) < 1e-4f);
    Ok("varianti diverse hanno moltiplicatori diversi", fabsf(k0 - k3) > 1e-3f);

    /* perAltezza falso guarda il lato XZ maggiore: e' il caso dei massi, larghi
     * e bassi, dove tarare sull'altezza darebbe un sasso gigante. */
    float kl = MeshGroupScale(&gr[3], 2.0f, false);  /* lati 1,0 x 0,8 */
    Ok("senza perAltezza si tara sul lato XZ maggiore", fabsf(kl - 2.0f) < 1e-4f);

    BoundingBox nullo = Box(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    MeshGroup gn = { 0, 1, nullo };
    Ok("un ingombro degenere non divide per zero",
       fabsf(MeshGroupScale(&gn, 1.4f, true) - 1.0f) < 1e-4f);
```

- [ ] **Passo 2: lanciarle e vederle fallire**

Esegui: `make prove`
Atteso: errore di compilazione, `MeshGroupOrigin` non dichiarata.

- [ ] **Passo 3: dichiarare le tre funzioni in `src/meshgroup.h`**

Prima di `#endif`:

```c
/* L'origine da portare a zero: centro XZ dell'ingombro e MINIMO Y. Il centro
 * in Y metterebbe mezza pianta sottoterra: le piante stanno appoggiate. */
Vector3 MeshGroupOrigin(const MeshGroup *g);

/* Sposta i vertici di -o. 'v' e' l'array di raylib: 3 float per vertice.
 * Si fa una volta al caricamento e non per fotogramma - l'alternativa era un
 * uniform per lotto e una sottrazione per vertice a ogni disegno. */
void MeshGroupRecenter(float *v, int vertexCount, Vector3 o);

/* Il moltiplicatore che porta il gruppo alla dimensione voluta in metri:
 * sull'altezza se perAltezza, altrimenti sul lato XZ maggiore. Torna 1 se
 * l'ingombro e' degenere. */
float MeshGroupScale(const MeshGroup *g, float voluto, bool perAltezza);
```

- [ ] **Passo 4: implementarle in `src/meshgroup.c`**

In fondo al file:

```c
Vector3 MeshGroupOrigin(const MeshGroup *g)
{
    Vector3 o;
    o.x = (g->box.min.x + g->box.max.x) * 0.5f;
    o.y = g->box.min.y;
    o.z = (g->box.min.z + g->box.max.z) * 0.5f;
    return o;
}

void MeshGroupRecenter(float *v, int vertexCount, Vector3 o)
{
    if (v == NULL) return;
    for (int i = 0; i < vertexCount; i++) {
        v[i * 3 + 0] -= o.x;
        v[i * 3 + 1] -= o.y;
        v[i * 3 + 2] -= o.z;
    }
}

float MeshGroupScale(const MeshGroup *g, float voluto, bool perAltezza)
{
    float dim = perAltezza
                ? g->box.max.y - g->box.min.y
                : fmaxf(g->box.max.x - g->box.min.x, g->box.max.z - g->box.min.z);
    return (dim > 1e-4f) ? voluto / dim : 1.0f;
}
```

- [ ] **Passo 5: lanciare le prove e vederle passare**

Esegui: `make prove`
Atteso: `build/prove/varianti` tutto `ok`.

- [ ] **Passo 6: sabotare**

Una per volta, poi rimettere a posto:

1. in `MeshGroupOrigin`, metti `o.y = (min.y + max.y) * 0.5f` → deve fallire "l'origine e' il MINIMO Y";
2. in `MeshGroupRecenter`, sottrai anche in Y con il centro (cioè `-= o.y * 2.0f`) → deve fallire "il ricentraggio non schiaccia la variante a terra";
3. in `MeshGroupScale`, scambia `max.y - min.y` con il lato XZ → deve fallire "varianti di altezza diversa arrivano alla stessa taglia";
4. togli la guardia `dim > 1e-4f` → l'ingombro degenere deve dare `inf` e far fallire l'ultimo controllo.

- [ ] **Passo 7: commit**

```bash
git add src/meshgroup.h src/meshgroup.c tools/prove/varianti.c
git commit -m "Portare una variante all'origine e alla taglia dichiarata"
```

---

### Task 3: Un gruppo di lotti per un sottoinsieme di mesh

**File:**
- Modifica: `src/instancing.h:70-76` (il blocco `InstModel`)
- Modifica: `src/instancing.c:240-263` (`InstModelCreate`)
- Modifica: `tools/prove/instancing.c`

**Interfacce:**
- Consuma: `InstCreate()`, `InstModelFree()`, già esistenti.
- Produce: `bool InstModelCreateSubset(InstModel *im, Model m, const int *meshIdx, int n)`. `meshIdx` sono indici dentro `m.meshes`; vale il tutto-o-niente di `InstModelCreate()`. `InstModelCreate()` resta e diventa il caso "tutte le mesh".

- [ ] **Passo 1: scrivere la prova che fallisce**

`tools/prove/instancing.c` ha già finestra, shader e mesh. Aggiungi in `main()`, prima del `return`, dopo i controlli esistenti:

```c
    /* --- Un sottoinsieme di mesh ------------------------------------------
     * Un set di varianti e' un modello solo: le mesh di una variante vanno in
     * un gruppo di lotti, quelle delle altre no. Se il sottoinsieme venisse
     * ignorato, il gioco disegnerebbe tutte le varianti insieme - che e'
     * esattamente il difetto da togliere. */
    Model due = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    InstModel sub;
    int uno[1] = { 0 };
    Ok("un sottoinsieme di una mesh si crea",
       InstModelCreateSubset(&sub, due, uno, 1) && sub.n == 1);
    InstModelFree(&sub);

    int fuori[1] = { 7 };
    Ok("un indice fuori dal modello non si crea, e non e' un crollo",
       !InstModelCreateSubset(&sub, due, fuori, 1) && sub.n == 0);

    int vuoto[1] = { 0 };
    Ok("un sottoinsieme vuoto non si crea",
       !InstModelCreateSubset(&sub, due, vuoto, 0) && sub.n == 0);
    UnloadModel(due);
```

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: errore di compilazione, `InstModelCreateSubset` non dichiarata. (Se la macchina non ha una GPU la prova uscirebbe 77 *a runtime*, ma la compilazione fallisce comunque: è il fallimento che serve.)

- [ ] **Passo 3: dichiararla in `src/instancing.h`**

Sotto `bool InstModelCreate(InstModel *im, Model m);`:

```c
/* Come InstModelCreate, ma solo per le mesh elencate in meshIdx: serve ai set
 * di varianti, dove un modello contiene piu' individui affiancati e se ne
 * disegna uno solo. Vale lo stesso tutto-o-niente. */
bool InstModelCreateSubset(InstModel *im, Model m, const int *meshIdx, int n);
```

- [ ] **Passo 4: implementarla in `src/instancing.c`**

Sostituisci l'attuale `InstModelCreate()` con le due funzioni:

```c
bool InstModelCreateSubset(InstModel *im, Model m, const int *meshIdx, int n)
{
    im->b = NULL;
    im->n = 0;
    if (meshIdx == NULL || n <= 0) return false;

    im->b = (InstBatch **)MemAlloc((unsigned int)(n * (int)sizeof(InstBatch *)));
    if (im->b == NULL) return false;

    for (int k = 0; k < n; k++) {
        int i = meshIdx[k];
        if (i < 0 || i >= m.meshCount) { im->n = k; InstModelFree(im); return false; }

        int mat = (m.meshMaterial != NULL) ? m.meshMaterial[i] : 0;
        if (mat < 0 || mat >= m.materialCount) mat = 0;

        im->b[k] = InstCreate(m.meshes[i], m.materials[mat]);
        if (im->b[k] == NULL) {          /* tutto o niente */
            im->n = k;
            InstModelFree(im);
            return false;
        }
    }
    im->n = n;
    return true;
}

bool InstModelCreate(InstModel *im, Model m)
{
    im->b = NULL;
    im->n = 0;
    if (m.meshCount <= 0) return false;

    int *idx = (int *)MemAlloc((unsigned int)(m.meshCount * (int)sizeof(int)));
    if (idx == NULL) return false;
    for (int i = 0; i < m.meshCount; i++) idx[i] = i;

    bool ok = InstModelCreateSubset(im, m, idx, m.meshCount);
    MemFree(idx);
    return ok;
}
```

- [ ] **Passo 5: lanciare prove e compilazione**

Esegui: `make prove && make`
Atteso: le prove passano (o escono 77 senza GPU), `make` non stampa avvisi, Linux e Windows.

- [ ] **Passo 6: sabotare**

1. in `InstModelCreateSubset`, ignora `meshIdx` e usa `m.meshes[k]` → deve fallire "un indice fuori dal modello non si crea";
2. togli il controllo `n <= 0` → deve fallire "un sottoinsieme vuoto non si crea".

- [ ] **Passo 7: commit**

```bash
git add src/instancing.h src/instancing.c tools/prove/instancing.c
git commit -m "Un gruppo di lotti per un sottoinsieme di mesh"
```

---

### Task 4: Le varianti in gioco

**File:**
- Modifica: `src/world.h:54-64` (i campi dei modelli esterni)
- Modifica: `src/world.c:255-301` (`LoadExtProps`), `src/world.c:500-504` (lo scarico), `src/world.c:759-782` (il ripiego in `DrawProp`), `src/world.c:860-897` (`PropBatchBegin`, `PropBatchFlush`, `PropBatchAdd`)
- Modifica: `tools/prove/varianti.c`

**Interfacce:**
- Consuma: tutto `meshgroup.h`, e `InstModelCreateSubset()` dal Task 3.
- Produce: `PropVariants` in `world.h` e `static int PropVariantOf(const Prop *p, int n)` in `world.c`. Sparisce `float extPropScale[PROP_COUNT]`: la scala ora è per variante.

- [ ] **Passo 1: scrivere la prova che fallisce**

Aggiungi in fondo a `tools/prove/varianti.c`. Serve includere anche `world.c`, perché `PropVariantOf` è `static` — come fa `tools/prove/scale.c` con `ResolveHouse`. Metti l'include **in cima al file**, dopo quello di `meshgroup.c`:

```c
#include "../../src/world.c"
```

e i controlli prima di `return ProveEsito();`:

```c
    /* --- La scelta della variante -----------------------------------------
     * Funzione PURA della posizione, come HouseShapeOf: disegno, ombra e
     * collisione la richiamano e non possono divergere, e il mondo cotto non
     * cambia di un byte. */
    Prop q;
    memset(&q, 0, sizeof q);
    q.type = PROP_BUSH;
    q.scale = 1.0f;
    q.pos = (Vector3){ 12.5f, 0.0f, 41.25f };
    Ok("la stessa posizione da' sempre la stessa variante",
       PropVariantOf(&q, 4) == PropVariantOf(&q, 4));
    Ok("un modello a una variante sceglie sempre la prima",
       PropVariantOf(&q, 1) == 0);

    /* Tutte le varianti devono comparire, o meta' dell'asset e' peso morto. */
    int conta[4] = { 0, 0, 0, 0 }, fuoriRange = 0;
    for (int i = 0; i < 2000; i++) {
        q.pos = (Vector3){ (float)i * 1.37f, 0.0f, (float)(i % 53) * 2.11f };
        int v = PropVariantOf(&q, 4);
        if (v < 0 || v >= 4) fuoriRange++;
        else conta[v]++;
    }
    Ok("la variante resta dentro l'array", fuoriRange == 0);
    Ok("su 2000 posizioni compaiono tutte e quattro le varianti",
       conta[0] > 0 && conta[1] > 0 && conta[2] > 0 && conta[3] > 0);
    Ok("nessuna variante si prende piu' di meta' del bosco",
       conta[0] < 1000 && conta[1] < 1000 && conta[2] < 1000 && conta[3] < 1000);
```

Aggiorna la riga di `varianti` nel `Makefile`: ora la prova include `world.c`, quindi servono gli stessi moduli della prova `scale`.

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/varianti.c \
	      $(SRC_DIR)/fmath.c $(SRC_DIR)/light.c $(SRC_DIR)/instancing.c \
	      $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c \
	      $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/varianti
```

`meshgroup.c` resta incluso dalla prova e **non** va aggiunto alla riga di collegamento, o i simboli si duplicano.

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: errore di compilazione, `PropVariantOf` non definita.

- [ ] **Passo 3: cambiare `src/world.h`**

Aggiungi `#include "meshgroup.h"` fra gli include, e sostituisci i tre campi `extPropScale` / `propBatch` con:

```c
    /* Un asset puo' contenere piu' individui affiancati: meta' del catalogo
     * vegetale Poly Haven e' fatta cosi'. Ogni variante ha il suo gruppo di
     * lotti, la sua scala - tarata sul PROPRIO ingombro fino alla dimensione
     * dichiarata in gExtProp, cosi' il raggio di collisione cotto nel mondo
     * resta valido - e le sue mesh, che servono al ripiego non instanziato.
     * n == 0 vuol dire nessun modello esterno per questo tipo. */
    PropVariants propVar[PROP_COUNT];
```

e definisci il tipo sopra `World`:

```c
typedef struct {
    InstModel *batch;     /* un gruppo di lotti per variante */
    float     *scala;     /* moltiplicatore per variante     */
    int       *meshIdx;   /* indici delle mesh, raggruppati  */
    MeshGroup *gruppo;    /* first/count dentro meshIdx      */
    int        n;
} PropVariants;
```

- [ ] **Passo 4: caricare a varianti in `LoadExtProps()`**

In `src/world.c`, sostituisci il blocco che va da `BoundingBox bb = GetModelBoundingBox(m);` fino alla `TraceLog` finale con:

```c
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

        /* Oltre MESHGROUP_MAX mesh il raggruppamento si arrende: si torna a
         * trattare il modello come un individuo solo, che e' il comportamento
         * di prima delle varianti. */
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
                MeshGroupRecenter(me->vertices, me->vertexCount, o);
                UpdateMeshBuffer(*me, 0, me->vertices,
                                 me->vertexCount * 3 * (int)sizeof(float), 0);
            }

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
```

Sopra `LoadExtProps()` metti la funzione di scarico, che serve anche a `WorldUnload`:

```c
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
```

In `WorldUnload` (`src/world.c:500-504`) sostituisci `InstModelFree(&w->propBatch[t]);` con `FreePropVariants(&w->propVar[t]);`.

- [ ] **Passo 5: scegliere e disegnare**

Sopra `PropBatchBegin()` in `src/world.c`:

```c
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
```

`PropBatchBegin` e `PropBatchFlush` girano su tutte le varianti — il passaggio riempie i lotti di quella scelta e gli altri restano vuoti, che costa un `InstFlush` con zero istanze:

```c
    for (int t = 0; t < PROP_COUNT; t++)
        for (int g = 0; g < w->propVar[t].n; g++)
            InstModelBegin(&w->propVar[t].batch[g], tint);
```

```c
    for (int t = 0; t < PROP_COUNT; t++)
        for (int g = 0; g < w->propVar[t].n; g++)
            InstModelFlush(&w->propVar[t].batch[g]);
```

`PropBatchAdd`:

```c
static bool PropBatchAdd(World *w, const Prop *p)
{
    PropVariants *pv = &w->propVar[p->type];
    if (pv->n == 0 || p->taken) return false;

    int v = PropVariantOf(p, pv->n);
    if (!InstModelReady(&pv->batch[v])) return false;

    float k = p->scale * pv->scala[v];
    InstModelAdd(&pv->batch[v], p->pos, p->rot, (Vector3){ k, k, k });
    return true;
}
```

Il ripiego non instanziato in `DrawProp` (`src/world.c:768-782`): `DrawModelEx` disegnerebbe tutte le varianti, che ora sono ricentrate una sopra l'altra. Sostituisci il blocco `if (w->hasExtProp[p->type]) { ... }` con:

```c
    if (w->hasExtProp[p->type] && w->propVar[p->type].n > 0) {
        if (p->taken) return;

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

            /* Il materiale e' una copia: si tinge questa, non quella del
             * modello, o la tinta del ciclo giorno/notte si accumulerebbe. */
            Material mm = mo->materials[mat];
            mm.maps[MATERIAL_MAP_DIFFUSE].color = Shade(WHITE, tint);
            DrawMesh(mo->meshes[mi], mm, mt);
        }

        if (cut > 0.0f) LightSetAlphaCut(0.0f);
        return;
    }
```

- [ ] **Passo 6: lanciare prove e compilazione**

Esegui: `make prove && make && make valida`
Atteso: `varianti` tutto `ok`, `scale` invariata, `make` senza avvisi su Linux e Windows, `make valida` invariato.

- [ ] **Passo 7: sabotare**

1. in `PropVariantOf`, torna sempre `0` → deve fallire "compaiono tutte e quattro le varianti";
2. togli la guardia `(v >= n) ? n - 1 : v` e forza `v = n` → deve fallire "la variante resta dentro l'array";
3. cambia il sale da 91 a 77 → le prove passano ancora (è corretto: provano la distribuzione, non il valore), ma verifica a mano che la variante non sia più correlata alla forma delle case guardando due prop nello stesso punto. Se non riesci a costruire il caso, lascia il sale a 91 e annotalo: è una decisione, non un controllo automatico.

- [ ] **Passo 8: commit**

```bash
git add src/world.h src/world.c tools/prove/varianti.c Makefile
git commit -m "Una variante per prop, scelta dalla posizione"
```

---

### Task 5: Un set di varianti vero in gioco

**File:**
- Modifica: `src/world.c:164-171` (la tabella `gExtProp`)
- Modifica: `docs/03-asset-pubblici.md`, `docs/01-architettura.md`, `docs/06-stato-e-prossimi-passi.md`
- Scarica in: `assets/models/` (i file scaricati non si committano se `.gitignore` li esclude — controllalo prima con `git status`)

**Interfacce:**
- Consuma: tutto il Task 4.
- Produce: nessuna API nuova.

- [ ] **Passo 1: scaricare il set**

```bash
./tools/fetch_assets.sh polyhaven shrub_02 bush
```

Atteso: `assets/models/bush.gltf` con il `.bin` e le texture accanto. `gExtProp[PROP_BUSH]` dichiara già `assets/models/bush.glb`, e `TrovaModello()` prova da sé l'altra estensione: non serve toccare la tabella se il nome è `bush`.

- [ ] **Passo 2: guardare il registro**

```bash
./frostmark 2>&1 | grep "modello esterno"
```

Atteso: una riga tipo `modello esterno assets/models/bush.gltf (N mesh, 4 varianti, x… -> 1.4 m), a lotti`. **Se dice `1 variante`, il raggruppamento non ha separato i cespugli**: prima di andare avanti, stampa gli ingombri delle mesh e verifica se si toccano davvero. Non aggirare il problema alzando una tolleranza: lo spec dice contatto senza tolleranza, e se il caso reale lo smentisce va corretto lo spec.

- [ ] **Passo 3: guardare il gioco**

Esegui il gioco e cammina in un bosco. Tre cose, e la terza è quella che si dimentica:

1. dove c'è un cespuglio ce n'è **uno**, non quattro in miniatura allineati;
2. cespugli vicini hanno forme diverse, non solo rotazioni diverse;
3. **l'ombra** segue la variante disegnata — il passaggio d'ombra usa la stessa `PropVariantOf`, e se le due divergessero si vedrebbe l'ombra di un cespuglio diverso da quello che c'è.

- [ ] **Passo 4: aggiornare i documenti**

- `docs/03-asset-pubblici.md`: i set di varianti non sono più un asset da scartare. Aggiungi come si riconoscono e che il motore ne disegna uno per prop.
- `docs/01-architettura.md`: una sezione *Varianti* accanto a *Instancing*, con la regola del contatto XZ, il ricentraggio al caricamento e il perché la scelta è funzione della posizione.
- `docs/06-stato-e-prossimi-passi.md`: la domanda **A è chiusa**; restano B (spezzare le mesh) e C (i personaggi). Aggiorna anche il vincolo 3 dei "tre vincoli scoperti misurando": i set di varianti non sono più un ostacolo.
- `assets/CREDITS.md`: la riga dell'asset scaricato.

- [ ] **Passo 5: verifica finale**

```bash
make && make prove && make valida
```

Atteso: zero avvisi, prove verdi (o saltate), validazione invariata.

- [ ] **Passo 6: commit**

```bash
git add src/world.c docs assets/CREDITS.md
git commit -m "Un cespuglio per cespuglio, non quattro in miniatura"
```

---

## Note per chi esegue

- **Non toccare `TroppiVertici()`.** Spezzare le mesh oltre 65.535 vertici è la domanda B dello stato del progetto, ed è fuori da questo piano. Se un asset viene scartato per il tetto, scegline un altro.
- **Il percorso non instanziato non è codice morto:** gira quando `assets/shaders/` manca. Se lo rompi non se ne accorge nessuna prova, perché nessuna prova gira senza shader. Verificalo a mano rinominando `assets/shaders/scene_inst.vs` e riavviando il gioco: i cespugli devono restare uno per posizione.
- **Il mondo cotto non va rigenerato.** Se ti viene voglia di rigenerarlo, ti è sfuggito qualcosa: le varianti sono una funzione della posizione apposta per non toccare i dati.
