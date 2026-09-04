# Instancing e impostori — design

**Data:** 2026-09-04
**Fase:** 2 del piano "asset realistici" (fase 1: normal map, fatta; fase 3: gli asset)

## Perché

La fase 3 vuole sostituire i prop stilizzati da ~100 triangoli con asset
realistici da qualche migliaio. Il motore oggi non li reggerebbe, e questo
documento dice perché e cosa si cambia.

### La misura, prima di tutto

Baseline presa su un binario strumentato (copia dei sorgenti fuori dal repo),
giocatore fermo allo spawn, visuale che ruota di 360° per campionare tutte le
direzioni, 75 secondi:

| | migliore | peggiore |
|---|---|---|
| scena | 6,2 ms | 12,1 ms |
| — passaggio ombre | 3,7 ms | 3,9 ms |
| — passaggio principale | 2,4 ms | 6,0 ms |
| chiamate di disegno | 911 | 1.766 |
| prop disegnati | 497 | 1.179 |
| prop caricati | 5.918 | 5.918 |

Quattro conclusioni, tutte con un numero dietro:

1. **Il culling attuale basta.** Scarta l'80–92% dei prop caricati. Un frustum
   vero al posto del prodotto scalare darebbe poco: è **fuori ambito**.
2. **Il passaggio principale è limitato dalle chiamate, non dai triangoli.**
   6,0 ms per ~1.360 chiamate fa ~4,4 µs a chiamata, con mesh da ~100
   triangoli. Triplicando la densità si arriva a ~3.500 chiamate, cioè ~15 ms
   nel solo passaggio principale: fuori budget.
3. **C'è margine oggi.** 12,1 ms nel caso peggiore sono 82 fps. Si progetta,
   non si rattoppa.
4. **Il passaggio d'ombra costa 3,7 ms fissi con ~400 chiamate**, ~9 µs
   l'una — il doppio del principale, perché ogni cascata rifà i bind di
   materiale da capo.

### La prova che ha deciso il piano

Il sospetto era che i 3,7 ms delle ombre fossero riempimento: due render target
2048×2048 riempiti due volte per fotogramma. Misurato abbassando la
risoluzione:

| SHADOW_RES | riempimento | passaggio ombre |
|---|---|---|
| 2048 | 4,2 Mtexel × 2 | 3,61 – 3,79 ms |
| 1024 | 1,0 Mtexel × 2 | 3,70 – 4,09 ms |

**Quattro volte meno riempimento, stesso tempo.** Non è riempimento: sono le
chiamate. Alzare o abbassare la risoluzione delle ombre non sposta niente,
mentre l'instancing taglia entrambi i passaggi. Senza questa prova il piano
avrebbe speso lavoro sulla leva sbagliata.

## Obiettivo

60 fps con mesh molto più pesanti **e** più densità e distanza di oggi. Non
solo "sopravvivere agli asset realistici": anche alberi visibili oltre i 260 m
attuali e sottobosco più fitto.

## Decisioni

### Il dato d'istanza è di 28 byte, non 100

Ogni prop e ogni pezzo d'edificio è **posizione + rotazione attorno a Y +
scala**. Non esistono rotazioni libere in questo gioco. Quindi non si spedisce
una `mat4` per istanza (64 byte) più la matrice delle normali (36), ma due soli
attributi:

```glsl
in vec4 instPosYaw;   /* xyz: posizione; w: imbardata in radianti */
in vec3 instScale;
```

Il vertex shader ricostruisce da un `sin`/`cos` sia la matrice del modello sia
quella delle normali, ed **entrambe restano esatte anche con scala non
uniforme**. Serve davvero: la falda del tetto è scalata `(cell, cell·1,6,
cell·nz)` e le primitive procedurali lo sono quasi tutte. Un quarto della banda
della soluzione ovvia, e nessun caso sbagliato.

### Un VAO nostro per lotto

`DrawMeshInstanced()` di raylib attacca gli attributi d'istanza al VAO **della
mesh**, e alla fine cancella il VBO delle istanze senza azzerare i divisor
(`rmodels.c`, in fondo alla funzione). Disegnare poi la stessa mesh **senza**
instancing legge attributi ancora accesi, con divisor 1, puntati a un buffer
che non esiste più. I prop si disegnano tre volte per fotogramma — principale
più due cascate — quindi il caso non è teorico.

Il modulo crea perciò un **VAO separato per lotto**, che punta ai VBO della
mesh più il buffer d'istanza nostro. Il VAO della mesh resta intatto e i due
percorsi non si vedono nemmeno.

È anche il motivo per cui non si usa l'API di raylib: quella alloca e distrugge
un VBO **a ogni chiamata**, cioè ~30 `glGenBuffers`/`glDeleteBuffers` per
fotogramma.

## Architettura

### `src/instancing.c` / `.h`

Un lotto per coppia (mesh, materiale). Vive quanto il mondo; il buffer GPU è
persistente e cresce per raddoppi, mai distrutto per fotogramma.

```c
InstBatch *InstCreate(Mesh mesh, Material mat);
void       InstFree(InstBatch *b);

void InstBegin(InstBatch *b);                              /* svuota la lista */
void InstAdd(InstBatch *b, Vector3 pos, float yaw, Vector3 scale);
void InstFlush(InstBatch *b);                              /* carica e disegna */
```

`Begin`/`Add`/`Flush` si ripetono **per passaggio**, perché i tre passaggi
hanno liste diverse: il principale culla a cono, l'ombra a raggio.

### Shader

Nuovo `assets/shaders/scene_inst.vs`, con gli attributi d'istanza agli slot 5 e
6 — 0–4 sono già presi da posizione, UV, normale, colore e tangente.

`assets/shaders/scene.fs` **non cambia**: normal map, terna TBN e ombre valgono
già per entrambi i percorsi. La luce continua a vivere in un posto solo.

### Raggruppamento

`world.c` costruisce, per passaggio, un lotto per `(tipo di prop, livello di
dettaglio)`. Sei tipi di prop esterni, dieci pezzi d'edificio, più gli
impostori: una quindicina di lotti, contro le ~1.700 chiamate di oggi.

Il ciclo di culling non cambia: dove oggi chiama `DrawProp()`, chiama
`InstAdd()`; alla fine del ciclo si svuotano i lotti.

### Impostori

All'avvio ogni modello di prop viene reso su una `RenderTexture` piccola da
qualche angolazione attorno a Y. Oltre la distanza di soglia — oggi
`PROP_LOD_DIST` in `world.c`, 120 m, e da ritarare per tipo — il prop diventa un
quadrato che ruota **solo attorno a Y** per guardare la camera: non un billboard
pieno, che sugli alberi si vede "coricare" quando la camera sale.
Con l'instancing tutta la foresta lontana diventa **una chiamata**.

Nessuna dipendenza esterna e nessun file in più: gli impostori si generano da
qualunque asset, il che è il motivo per cui sono stati scelti al posto di un
decimatore (non c'è né Blender né pymeshlab, e i pacchetti realistici non
spediscono LOD).

**Il punto in cui si sbaglia:** gli impostori sono a ritaglio alfa, e il
passaggio d'ombra oggi esce alla prima riga (`if (depthOnly == 1) { finalColor
= vec4(1.0); return; }`). L'alfa dell'albedo va letta e il `discard` va fatto
**prima** di quel ritorno, altrimenti gli alberi lontani proiettano ombre
quadrate.

## Le quattro tappe

Ognuna si misura prima di passare alla successiva, con il banco strumentato già
scritto e lo stesso percorso di 75 secondi.

1. **Prop esterni** — modulo, shader, lotti per i sei tipi con modello `.glb`.
2. **Pezzi d'edificio** — `DrawHouse()` fa 19 o 45 chiamate per casa.
3. **Passaggio d'ombra** — è il costo fisso più grande.
4. **Impostori** — LOD lontano e distanza di vista maggiore.

Se dopo la prima tappa i numeri dicono che basta, le altre si **discutono**,
non si danno per fatte.

## Verifica

- **Equivalenza**: lo stesso prop reso instanziato e non instanziato in due
  `RenderTexture`, confrontate pixel a pixel. Stesso stile della prova della
  normal map della fase 1, che ha già dimostrato di discriminare (8 controlli
  su 23 falliscono sulla versione precedente).
- **Prestazioni**: numeri prima e dopo sullo stesso percorso, per ogni tappa.
- **Nessuna regressione visiva** nel percorso non instanziato: terreno,
  personaggi e primitive procedurali devono restare identici.

## Fuori ambito

- **Culling a frustum vero**: quello attuale scarta già il 90%.
- **Terreno**: già una chiamata per chunk.
- **Personaggi**: pochi e animati; l'instancing non li tocca.
- **Acqua e UI.**
- **Decimazione delle mesh**: gli impostori risolvono il campo lontano senza
  strumenti esterni. Un decimatore in `tools/` resta una possibilità futura per
  il campo medio, non parte di questa fase.

## Limiti noti che questa fase non risolve

- `UpdateModelAnimation()` aggiorna posizioni e normali ma non le tangenti: un
  personaggio animato con normal map avrà tangenti ferme alla posa di riposo
  (eredità della fase 1, documentato in `docs/01-architettura.md`).
- Il buffer d'istanza si aggiorna con `glBufferSubData` senza orphaning. Se il
  profilo mostrasse stalli di sincronizzazione, la mossa successiva è il doppio
  buffer — ma si misura prima.
- Il percorso procedurale senza asset (primitive `GenMesh*`) resta non
  instanziato. È la modalità degradata: funziona, non è veloce.
