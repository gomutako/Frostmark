# 06 — Stato del lavoro e prossimi passi

> **Se stai riprendendo il lavoro, parti da qui.** Questo file dice dove siamo
> arrivati, cosa è stato deciso e perché, e quali sono le due domande ancora
> aperte. Gli altri documenti spiegano *come funziona* il gioco; questo dice
> *a che punto siamo*.

**Ultimo aggiornamento:** 5 settembre 2026, commit `2dcb99c`.

---

## Dove siamo

Il progetto sta attraversando un piano in tre fasi, deciso per poter usare
asset 3D realistici al posto dei kit stilizzati:

| fase | cosa | stato |
|---|---|---|
| **1** | normal map nello shader | **fatta** |
| **2** | instancing e LOD | **fatta**, e la tappa LOD è stata *cancellata* dopo averla misurata |
| **3** | il gioco usa asset realistici | **in corso**: masso e cespuglio veri in gioco, gli alberi no |

### Fase 1 — normal map

`assets/shaders/scene.fs` legge le normal map da `texture2`. Tre trappole
risolte, tutte documentate in `docs/01-architettura.md`, sezione *Normal map*.
La più insidiosa: `GenMeshTangents()` di raylib 5.5 **ignora `mesh->indices`**,
quindi `light.c` ha il suo `BuildTangents()`. Non è un lusso — nessun asset
Poly Haven spedisce le tangenti nel file.

### Fase 2 — instancing

`src/instancing.c` disegna la stessa mesh molte volte con una chiamata sola.
Numeri misurati sullo stesso percorso di 75 secondi, prima e dopo:

| | prima | dopo |
|---|---|---|
| chiamate di disegno, picco | 1.766 | **144** |
| passaggio principale, peggiore | 6,0 ms | **2,0 ms** |
| scena, peggiore | 12,1 ms | **~6,7 ms** |

**La tappa 4 (impostori e LOD) è stata cancellata perché misurata inutile.**
Sostituendo tutti i prop con un asset vero il carico passa da 118.000 a **7,3
milioni di triangoli per fotogramma**, e la scena da 4,9 a 5,2 ms. Spingendo
fino a **92 milioni** di triangoli si arriva a 5,4 ms. A questa scala i
triangoli sono gratis: il collo di bottiglia erano le chiamate.

Dettagli e ragionamento in
`docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md`.

### Fase 3 — asset realistici

Fatto:

- **lotti per ogni coppia (mesh, materiale)**, non più solo per i modelli a
  mesh singola — nel catalogo Poly Haven la mesh singola è l'eccezione;
- **ritaglio dell'alfa**, con il `discard` prima dell'uscita anticipata del
  passaggio d'ombra, o l'ombra di una fronda resta un rettangolo;
- **le scale non si tarano più a mano**: `gExtProp` in `world.c` dichiara la
  dimensione voluta in metri e il moltiplicatore esce dall'ingombro vero;
- `LoadExtProps()` accetta `.glb` **e** `.gltf`;
- `fetch_assets.sh polyhaven <asset> <nome>` scarica dal catalogo leggendo gli
  URL dall'API;
- **in gioco c'è un masso vero**: `namaqualand_boulder_04`;
- **e un set di cespugli veri**: `shrub_02`, quattro individui in un file, uno
  per prop. Il motore riconosce gli individui dal contatto degli ingombri XZ e
  sceglie la variante dalla posizione — vedi *Varianti* in `docs/01`.

---

## I tre vincoli scoperti misurando

Non sono opinioni: ognuno è stato preso sbattendoci contro, e ognuno ha un
numero.

**1. Il tetto è nei vertici, non nei triangoli.** Il `Mesh` di raylib 5.5 tiene
gli indici in `unsigned short`: oltre **65.535 vertici per primitiva** li tronca,
avvisa con una riga in mezzo a centinaia, e il risultato non è un errore ma un
difetto visivo — gli indici si avvolgono e nascono triangoli che attraversano
l'oggetto. `boulder_01` è la trappola perfetta: 66.122 triangoli sembrano
innocui, sono 67.042 vertici. `LoadExtProps()` ora **scarta** i modelli oltre il
tetto e torna alla primitiva procedurale.

**2. Nessun albero del catalogo CC0 sta sotto quel tetto.** Il più vicino,
`quiver_tree_01`, manca per 3.787 vertici; `island_tree_02` ne ha 625.401 in una
primitiva sola. Non è una questione di specie o di bioma: **non ce n'è uno
caricabile**. Gli alberi restano quelli del kit.

**3. Metà del catalogo vegetale è fatto di *set di varianti*.** `shrub_02` sono
quattro cespugli diversi in fila su sei metri; `periwinkle_plant` sei piante
affiancate su 1,2. Era un ostacolo — caricati come un oggetto unico, dove
andava un cespuglio ne comparivano quattro in miniatura — e non lo è più: il
motore separa gli individui dal contatto degli ingombri XZ, li ricentra al
caricamento e ne disegna uno per prop. `shrub_02` entra in gioco come *4 mesh,
4 varianti*, e i quattro ingombri sono separati da 0,23, 0,27 e 0,29 m senza
bisogno di nessuna tolleranza. **Il vincolo si è rovesciato in un vantaggio**:
un set è quello che serve a un bosco.

---

## Le domande aperte

### A. La scelta delle varianti — **CHIUSA**

Fatta. `src/meshgroup.c` riconosce gli individui dentro un modello dal contatto
degli ingombri XZ, `LoadExtProps()` li ricentra e fa un gruppo di lotti per
variante, `PropVariantOf()` sceglie quale disegnare dalla posizione del prop.
In gioco c'è `shrub_02`: *4 mesh, 4 varianti, ×0,85 → 1,4 m, a lotti*, e su
otto cespugli vicini compaiono tutte e quattro le forme. Il mondo cotto non è
stato toccato, come previsto.

Restano fuori, e restano YAGNI finché non si vede il bisogno guardando il
bosco: i **pesi per variante** (una comune, tre rare) e le varianti dichiarate
a mano.

Il design è in
`docs/superpowers/specs/2026-09-05-varianti-prop-design.md`; il funzionamento in
`docs/01-architettura.md`, sezione *Varianti*.

### B. Spezzare le mesh oltre il tetto

È l'unico modo per usare gli alberi realistici: dividere una primitiva da
625.000 vertici in blocchi da 65.535, reindicizzando. Lavoro molto più grosso
di A, e da valutare se ne valga la pena — l'alternativa è tenere gli alberi
stilizzati e realistico tutto il resto.

### C. I personaggi

Sono **il costo dominante del fotogramma**: nel blocco d'ombra ci sono
`EntitiesDraw()` e `PlayerDraw()`, animati e disegnati una volta per cascata, e
togliendoli il passaggio scende da 3,3 a **0,26 ms**. Erano fuori ambito nella
fase 2 e restano il posto giusto dove guardare per il prossimo margine di
prestazioni. Nota nota: `UpdateModelAnimation()` aggiorna posizioni e normali
ma **non** le tangenti, quindi un personaggio animato con normal map avrebbe le
tangenti ferme alla posa di riposo.

---

## Come si verifica che tutto regga

```bash
make            # Linux e Windows, zero avvisi
make prove      # le prove, esce non-zero se qualcosa non torna
make valida     # dati e mondo cotto
```

`make prove` compila ed esegue ogni file in `tools/prove/`. Non c'è un
framework: una prova è un eseguibile che stampa una riga per controllo. Chi
esce 77 non ha trovato un contesto OpenGL e viene contata come saltata.

**Le prove sono state verificate sabotando il codice di proposito**, perché una
prova che non prende niente è peggio di nessuna prova. Da lì è uscita una cosa
che vale la pena ricordare: nella prova dell'instancing il **cubo non si accorge**
se la normale viene moltiplicata per la scala invece che divisa, perché le sue
normali sono versori sugli assi. Serve la sfera. Sta scritto nel file, o
qualcuno la toglierà credendola ridondante.

### Misurare le prestazioni

Non c'è un bersaglio del Makefile: si costruisce un binario strumentato da una
copia dei sorgenti fuori dal repo, si forza `GS_PLAY` in `GameInit()` **e** in
`GameNewWorld()`, si fa ruotare `g->player.yaw` per campionare tutte le
direzioni, e si gira 75 secondi. Tutti i numeri di questo documento vengono da
lì, sempre dallo stesso percorso.

---

## Documenti collegati

- `docs/01-architettura.md` — sezioni *Normal map*, *Instancing*, *Varianti* e
  *Le prove*
- `docs/03-asset-pubblici.md` — il catalogo misurato e come si sceglie un asset
- `docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md` — il
  perché della fase 2, con le misure che hanno deciso il piano
- `docs/superpowers/plans/2026-09-04-instancing.md` — il piano eseguito
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — il design della
  domanda A, e `docs/superpowers/plans/2026-09-05-varianti-prop.md` il piano
