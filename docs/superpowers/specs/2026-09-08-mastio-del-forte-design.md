# Il mastio del forte — design

**Data:** 2026-09-08
**Fase:** 4 del piano "asset realistici" (fasi 1, 2 e 3 fatte)
**Domanda che chiude:** la D di `docs/06-stato-e-prossimi-passi.md`, per la
sola torre

## Perché

Delle cinque cose che il gioco disegna, tre sono realistiche — massi, cespugli,
erba — e gli edifici portano materiali fotogrammetrici proiettati. Restano
stilizzati alberi, pini, cripta e **torre**. Delle quattro, la torre è l'unica
per cui esiste già un asset CC0 che sta dentro tutti i vincoli scoperti finora.

`modular_fort_01` di Poly Haven è un kit modulare vero: venti pezzi separati in
un file solo, nessuno vicino al tetto dei vertici, con materiali PBR di
scansione. Non è un albero da 625.000 vertici né una casa di villaggio, che nel
catalogo non esiste: è un pezzo di fortezza, ed è esattamente ciò che una torre
di guardia dovrebbe essere.

E c'è una scoperta che rende il lavoro corto: **il motore sa già caricare un
kit spedito in un file unico.** Il raggruppamento delle varianti, scritto per
`shrub_02`, ha letto i venti pezzi del forte da solo. Quello che non sa fare è
*scegliere quale pezzo va dove*, perché non è un sorteggio dalla posizione ma
logica di costruzione.

## Obiettivo

La torre di guardia al centro di ogni villaggio diventa **una torre tonda in
pietra fotogrammetrica**, alla scala vera del forte: un punto di riferimento
che si vede da lontano, al posto dei quattro pezzi Kenney impilati.

Resta una **scatola piena**: non ci si entra, come oggi. Il cortile in cui si
cammina è un'altra domanda, e non questa.

## L'asset, misurato

Misurato il 2026-09-08 sul `.gltf` a 1k scaricato dall'API di Poly Haven,
leggendo gli ingombri dagli accessori e applicando le trasformazioni dei nodi —
cioè quello che raylib fonde dentro i vertici al caricamento.

| | |
|---|---|
| primitive nel file | 45 |
| gruppi dalla regola XZ | **20** |
| vertici, primitiva peggiore | **4.148** (il tetto è 65.535) |
| triangoli in tutto | 28.218 |
| materiali PBR | **tre**: `wall`, `trim`, `plaster` |
| geometria + texture a 1k | 1,36 MB + 9,29 MB |

Il documento `06` diceva quattro materiali: sono tre. E `fir_tree_01`, per
confronto, è 478 MB di sola geometria: questo asset sta tre ordini di grandezza
sotto.

Il pezzo che serve è **`tower_round`**, il gruppo **12**:

| | |
|---|---|
| ingombro | **15,84 × 13,50 × 15,84 m** |
| mesh | 2 (`wall` e `trim`) |
| vertici | 4.544 |
| base | Y = 0 |

Accanto a case da 7,8 × 5,2 non è una torretta: è un mastio. È il motivo per
cui i venti pezzi non si prendono in blocco — i bastioni lunghi 14,8 m
farebbero un maniero, e il villaggio ha nove case in anello, non un castello.

### L'indice 12 è riproducibile

`MeshGroupSplit()` ordina i gruppi per `min.x` crescente e, a parità, per
`min.z` (`PrimaDi()` in `src/meshgroup.c`). Sui venti gruppi del forte ci sono
**due pareggi su `min.x`** — a 10,00 e a 19,00 — e in tutti e due la `min.z` li
separa. L'ordine è quindi totale e non dipende dal compilatore: il gruppo 12 è
`tower_round` a ogni esecuzione e su ogni piattaforma.

Che l'indice sia riproducibile non vuol dire che sia **stabile nel tempo**:
vedi *La guardia*, più sotto.

## Indirizzare "il pezzo *k* del file *X*"

Oggi `BUILD_FILES[i]` è un percorso, e un percorso è un pezzo intero. La riga
diventa:

```c
static const struct { const char *file; int pezzo; } BUILD_FILES[BUILD_PART_COUNT] = {
    [BUILD_WALL] = { "assets/models/town/wall.glb", -1 },
    ...
    [BUILD_KEEP] = { "assets/models/fort/modular_fort_01.gltf", 12 },
};
```

`pezzo = -1` significa "tutto il file", che è ciò che fanno le dieci righe di
oggi: nessuna cambia comportamento.

Per una riga con `pezzo >= 0`, `LoadBuildParts()` fa la stessa sequenza che
`LoadExtProps()` fa già alle varianti, e per la stessa ragione:

1. `MeshGroupSplit()` sugli ingombri delle mesh, per trovare i gruppi;
2. `MeshGroupOrigin()` e `MeshGroupRecenter()` + `UpdateMeshBuffer()` sulle sole
   mesh del gruppo scelto — raylib fonde le trasformazioni dei nodi dentro i
   vertici, quindi il pezzo porta cucito l'offset che lo mette in fila con gli
   altri diciannove, e va tolto una volta al caricamento invece che a ogni
   fotogramma;
3. `InstModelCreateSubset()` per il lotto, che prende già una lista di mesh.

Nessun modulo nuovo, nessuno strumento nuovo. La macchina esiste tutta: le
mancava solo un chiamante che dicesse *quale*.

**Una cache dei file.** Più righe possono nominare lo stesso file — non oggi,
ma la cripta della domanda E lo farà. Il file si carica una volta e le righe che
lo nominano si prendono il loro gruppo dallo stesso `Model`. Senza la cache,
usare due pezzi del forte vorrebbe dire tenerne in memoria quaranta.

### Perché non spezzare il file fuori dal gioco

L'alternativa era uno script che sputa venti `.glb` separati, lasciando il
runtime intatto. Scartata: aggiunge un passo di preparazione fra lo scaricare e
il giocare, e congela il kit in venti file da tenere allineati a mano. Il
progetto oggi carica gli asset **come li spedisce il catalogo** — è il motivo
per cui `TrovaModello()` prova `.glb` e `.gltf` — e questo lo romperebbe.

L'altra alternativa era portare la torre sul percorso degli ext prop, che ha
già le varianti e la scala metrica. Scartata perché non sa impilare: una torre
è una ricetta, e `DrawTower()` la scrive già.

## La taglia

Il pezzo si usa alla sua scala vera. Ma la taglia si **dichiara in metri**, come
fa `gExtProp`, e il moltiplicatore esce da `MeshGroupScale()` sull'ingombro
misurato al caricamento — non da una costante tarata a mano.

Per il mastio la dimensione che conta è l'**altezza**, dichiarata a **13,50 m**:
è l'ingombro vero dell'asset, quindi `MeshGroupScale()` esce a 1,00 e la torre
resta 15,84 m di diametro. Il numero è dichiarato lo stesso, e non sottinteso.

Non è cerimonia: `BUILD_CELL` vale 2,6 m e regge le case, un bastione da 8,5 m
no. E se Poly Haven ricuoce l'asset in unità diverse, una costante scritta a
mano darebbe una torre alta trenta metri senza dire niente, mentre una taglia
dichiarata la riporta a quella giusta.

## La collisione, da un numero solo

Qui c'è la trappola. Il cerchio che spinge fuori il giocatore usa `p->radius`,
che è **cotto nel mondo**: `worldgen.c` emette 3,0 m per `PROP_TOWER`. Una
torre larga 15,84 con un raggio di 3,0 vuol dire camminare dentro il muro, e
ricuocere il mondo non basta — i mondi già salvati resterebbero a 3,0.

Quindi `PROP_TOWER`, **quando il mastio è caricato**, ignora il raggio cotto.
Non è un'eccezione nuova: la casa fa già esattamente questo, salta il cerchio e
va sui muri (`WorldResolveCollision`), perché la sua forma la decide la ricetta
e non il dato.

La torre è **tonda**, quindi la forma onesta è un cilindro — raggio 7,92 m,
altezza 13,50 — e non la scatola di oggi, che occluderebbe quattro angoli vuoti.
Il cilindro nel codice c'è già: `RayTrunk()`, scritto per i fusti degli alberi,
sostituisce il `RayBox()` nel taglio della camera.

**Un numero, due usi.** La spinta del giocatore e il taglio della camera leggono
la stessa funzione, per la stessa ragione per cui `StairTop()` serve sia a chi
cammina sulla rampa sia a chi ci sbatte contro: se divergessero, la camera
entrerebbe in un muro che il giocatore non può attraversare.

Un limite dichiarato: `RayTrunk()` torna solo l'ingresso nel cilindro, quindi
con l'occhio *dentro* non taglia niente. Su un volume pieno non ci si arriva, e
se ci si arrivasse il comportamento sarebbe quello di oggi.

### Il tetto che fissa la taglia

L'NPC `elder` nasce a **14 m** dal centro del villaggio (`GenTownNpcs` in
`tools/worldgen.c`), e gli NPC sono cotti nel mondo come i prop. Con raggio 7,92
restano 6 m di margine. **La semiampiezza del mastio non può superare 8 m**, o
un personaggio nasce dentro la pietra: il pezzo più largo del catalogo è 15,84 e
ci sta, di un soffio. È un vincolo da rileggere se un giorno si sceglie un pezzo
diverso.

## I materiali

`tower_round` ha **UV vere** e porta le sue tre mappe PBR: niente proiezione.
È l'opposto dei pezzi Kenney, le cui coordinate stanno tutte in una cella della
tavolozza e per cui esiste `LightSetProjection()`.

Oggi un pezzo senza riga in `gBuildMat` esce con un avviso — *"pezzo %d senza
riga in gBuildMat"* — perché l'assenza è quasi sempre una riga dimenticata.
Qui l'assenza è la scelta giusta, quindi `BuildMat` prende un campo esplicito:

```c
typedef struct { const char *nome; float tile; int mode; bool uvVere; } BuildMat;
```

`uvVere` a `true` dice "questo pezzo campiona le sue UV, e non è una
dimenticanza": niente proiezione e niente avviso. Un pezzo senza né `nome` né
`uvVere` resta il caso sospetto che merita di essere gridato.

## Il ripiego

Il mastio **non entra** nel "tutti o nessuno" dei dieci pezzi degli edifici.
Quel controllo esiste perché mezza casa è peggio di una scatola; una torre
Kenney invece è un edificio intero e giusto, solo stilizzato.

Se `modular_fort_01` manca, o non si carica, o non supera la guardia qui sotto:
la torre resta i quattro pezzi di oggi, con il raggio cotto di 3,0 m e la
scatola della camera. Non è un errore, ed è la stessa promessa di sempre — il
gioco funziona senza `assets/`.

## La guardia contro l'asset ricotto

L'indice 12 è **posizionale**. Se Poly Haven ricuoce il file e riordina i pezzi,
il 12 diventa un muro, e nessuno se ne accorge: è la stessa famiglia di difetti
di `boulder_01`, dove 66.122 triangoli sembravano innocui ed erano 67.042
vertici.

Quindi la riga dichiara anche **l'ingombro atteso**, e al caricamento si
confronta con quello misurato. Fuori tolleranza — un quinto per lato — si
avvisa con i due ingombri e si ripiega sui pezzi Kenney.

Costa tre righe e converte un difetto visivo silenzioso in una riga di log. È
lo stesso ragionamento di `TroppiVertici()`: meglio una torre onesta di un muro
messo dove va una torre.

## Da dove viene il file

`./tools/fetch_assets.sh` prende un sottocomando nuovo che scarica
`modular_fort_01` a 1k in `assets/models/fort/`, con il suo `.bin` e le sue
texture, e aggiunge la riga in `assets/CREDITS.md` — come già fa per i modelli e
per i materiali. La cartella è sua perché il `.gltf` nomina il `.bin` e le
texture per come stanno nel pacchetto, e rinominarli lo romperebbe.

## Cosa non cambia

Il mondo cotto non si tocca: la torre resta un prop nella stessa posizione, con
lo stesso tipo e lo stesso raggio scritto nel file. Cambia solo cosa il motore
ne fa. Le case, i loro materiali proiettati, le varianti dei prop e
l'instancing restano dove sono.

## Le prove

Una prova senza contesto grafico, come `scale` e `varianti`, che gira ovunque:

- **la selezione per indice** pesca il gruppo giusto fra i gruppi trovati, e
  l'ordinamento con un pareggio su `min.x` lo risolve sulla `min.z` — il caso
  che il forte contiene davvero, due volte;
- **il ricentraggio tocca solo il gruppo scelto**: le mesh degli altri
  diciannove restano dove sono, o disegnandone due si vedrebbero a chilometri
  di distanza;
- **la guardia dell'ingombro** scarta un gruppo della taglia sbagliata e accetta
  quello giusto dentro tolleranza;
- **il numero unico della collisione**: la stessa funzione che spinge il
  giocatore dà il raggio al cilindro della camera, con e senza il mastio
  caricato.

### E poi i sabotaggi

In questo progetto la regola ha già ripagato più volte, e tre difetti su prove
che passavano li hanno trovati i sabotaggi e non le revisioni. Qui vanno provati
almeno:

- **togliere il criterio `min.z`** dall'ordinamento: con due pareggi veri nel
  file, la prova deve cadere;
- **spostare l'indice di uno**: se passa, la prova non guarda *quale* gruppo
  ha preso, ma solo che ne abbia preso uno;
- **allargare la tolleranza della guardia a metà**: deve smettere di distinguere
  la torre da un bastione;
- **ricentrare tutti i gruppi invece di uno**: la prova deve accorgersi che le
  mesh degli altri si sono mosse.

Una prova che passa dopo il sabotaggio non prova niente e va riscritta, non
tenuta.

## Fuori ambito

- **Gli altri diciannove pezzi.** Questa chiude la domanda D per la sola torre.
  Le mura, la porta `large_castle_door` e il cortile in cui si cammina restano
  aperti.
- **Il cortile in cui si entra.** Vorrebbe collisione a segmenti con un varco,
  come le case, e la torre resta una scatola piena.
- **La cripta** (domanda E) — ma eredita tutta la macchina: comporla di pezzi
  vorrà dire scrivere righe in `BUILD_FILES` con un indice, e basta.
- **Ricuocere il mondo** per un raggio diverso: il raggio cotto resta quello che
  è, e il codice lo scavalca solo quando il mastio c'è.
- **I venti pezzi caricati per usarne due.** Costa 1,29 MB di geometria e le
  texture dei tre materiali. Si paga e si dichiara: è il prezzo di caricare il
  kit come lo spedisce il catalogo, e la cache dei file lo rende un prezzo che
  si paga una volta sola anche quando i pezzi usati saranno cinque.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md` — la domanda D e i vincoli misurati
- `docs/03-asset-pubblici.md` — il catalogo misurato e come si sceglie un asset
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — il
  raggruppamento delle mesh, che questa riusa per intero
- `docs/superpowers/specs/2026-09-06-materiali-triplanari-design.md` — la
  proiezione, che questa **non** usa, e il perché
