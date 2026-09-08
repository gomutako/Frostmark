# 06 — Stato del lavoro e prossimi passi

> **Se stai riprendendo il lavoro, parti da qui.** Questo file dice dove siamo
> arrivati, cosa è stato deciso e perché, e quali sono le domande ancora
> aperte. Gli altri documenti spiegano *come funziona* il gioco; questo dice
> *a che punto siamo*.

**Ultimo aggiornamento:** 8 settembre 2026, dopo il mastio, il tumulo e il
sottobosco.

> **Il mondo è stato ricotto e il risultato è nel repository.** I prop del
> sottobosco nascono nel mondo cotto, e `assets/world/` è versionata: chi
> aggiorna li riceve e non deve lanciare niente. Chi invece **cambia la
> generazione** deve ricuocere *e committare* `props.bin`, o il suo mondo e
> quello del repository divergono in silenzio.

---

## Dove siamo

Le prime due fasi del piano — normal map nello shader, poi instancing — sono
chiuse da tempo e non si toccano più. Quello che resta aperto è un obiettivo
solo: **sostituire ogni oggetto stilizzato del gioco con uno realistico.**

Non è un progetto solo. Guardandolo da vicino si divide in sei pezzi con
blocchi diversi. Uno — i personaggi — non si risolve affatto con Poly Haven,
perché quel catalogo è fatto di scansioni statiche; e per due, torre e cripta,
la risposta non è stata trovare l'oggetto ma **comporlo**:

| # | pezzo | stato |
|---|---|---|
| 1 | **erba** | **fatto** — `celandine_01`, cinque varianti |
| 2 | **alberi e pini** | **risolta diversamente** — restano stilizzati, ma il suolo sotto è vero. È la domanda **B** |
| 3 | **edifici** | **fatto** — materiali proiettati sui dieci pezzi modulari |
| 4 | **torre** | **fatto** — `tower_round`, il pezzo 12 dei venti di `modular_fort_01` |
| 5 | **cripta** | **fatto** — un tumulo di massi con un varco. È la domanda **E** |
| 6 | **personaggi** | aperto, il più grosso: serve un'altra fonte. È la domanda **C** |

### Cosa è realistico in gioco, oggi

- **massi**: `namaqualand_boulder_04`, ×0,87 → 2,2 m;
- **cespugli**: `shrub_02`, un set di quattro individui in un file — *4 mesh, 4
  varianti, ×0,85 → 1,4 m*, uno per prop;
- **erba**: `celandine_01`, *5 mesh, 5 varianti, ×2,18 → 0,6 m*. È raccoglibile,
  e il fiore giallo dell'asset fa da segnale al posto della pallina non tinta
  che aveva il modello procedurale;
- **edifici**: i dieci pezzi modulari di casa e torre portano quattro materiali
  fotogrammetrici — assiti scuri su muro, porta e finestra, scandole sul tetto,
  assi chiare su pavimento e scala, pietra sui quattro pezzi della torre;
- **torre**: `tower_round`, il **pezzo 12 dei venti** di `modular_fort_01` —
  *15,84 × 13,50 × 15,84 m alla scala vera, 2 mesh, 4.544 vertici, tre materiali
  PBR*. Non è più una torretta da 3,2 m ma un mastio che si vede da lontano, e
  la collisione lo segue: cilindro di raggio 7,92, che scavalca il raggio di
  3,0 cotto nel mondo;
- **cripta**: un **tumulo** di quindici massi di `rock_moss_set_01` in anello di
  5,5 m con un varco da 2,99 — *6 mesh, 6 varianti, ×0,83 → 2,8 m per masso* —
  e `gothic_statue` a fianco dell'ingresso, portata a 3 m. Ci si entra: la
  collisione è un cerchio per masso, e il centro resta libero perché è lì che
  nasce il boss;
- **sottobosco**: ceppi, tronchi caduti, radici, rami e scaglie di corteccia —
  cinque tipi, **35.653 prop** su un mondo che ne conta ora **204.386** contro i
  168.733 di prima. Il tronco è l'unico solido, ed è due cerchi sul suo asse.

### Cosa è ancora stilizzato

**Alberi, pini e personaggi.** I primi due per il vincolo 1 qui sotto, i
personaggi perché sono riggati e animati e Poly Haven non ne ha nessuno. Torre e
cripta sono uscite da questo elenco per la stessa ragione: quando l'oggetto non
esiste come scansione, **si compone**. Una casa di villaggio il catalogo non ce
l'ha, ma ha una fortezza; una cripta nemmeno, ma ha dei massi.

---

## I vincoli scoperti misurando

Non sono opinioni: ognuno è stato preso sbattendoci contro, e ognuno ha un
numero.

**1. Il tetto è nei vertici, non nei triangoli.** Il `Mesh` di raylib 5.5 tiene
gli indici in `unsigned short`: oltre **65.535 vertici per primitiva** li
tronca, avvisa con una riga in mezzo a centinaia, e il risultato non è un errore
ma un difetto visivo — gli indici si avvolgono e nascono triangoli che
attraversano l'oggetto. `boulder_01` è la trappola perfetta: 66.122 triangoli
sembrano innocui, sono 67.042 vertici. `LoadExtProps()` **scarta** i modelli
oltre il tetto e torna alla primitiva procedurale.

**2. Il vincolo sugli alberi non è il tetto: è il peso.** Misurati tutti e 145 i
modelli vegetali del catalogo — leggendo gli accessori dei `.gltf`, senza
scaricare un `.bin` — **109 stanno sotto il tetto**, quindi la vecchia frase
«non ce n'è uno caricabile» era falsa alla lettera.

Ma quelli che ci stanno sono **piccoli**: `quiver_tree_02` entra con 16.530
vertici di margine ed è alto **1,47 m**, `quiver_tree_01` 2,72 m, `island_tree_02`
3,41 m con 625.401 vertici. E quelli veri non sono appena sopra il tetto:
`fir_tree_01` è 487 MB, `pine_tree_01` **958 MB**.

Il catalogo non ha alberi grandi ed economici: ha **alberelli costosissimi**.
Reindicizzare le mesh risolverebbe gli indici e non il peso, quindi sbloccherebbe
**un asset solo**, alto due metri e settanta e di specie desertica.

**3. Metà del catalogo vegetale è fatto di *set di varianti*, e non è più un
ostacolo.** `shrub_02` sono quattro cespugli diversi in fila su sei metri.
Caricati come un oggetto unico, dove andava un cespuglio ne comparivano quattro
in miniatura. Ora il motore separa gli individui dal contatto degli ingombri XZ,
li ricentra al caricamento e ne disegna uno per prop, scelto dalla posizione. I
quattro ingombri di `shrub_02` sono separati da 0,23, 0,27 e 0,29 m, senza
bisogno di nessuna tolleranza. **Il vincolo si è rovesciato in un vantaggio**:
un set è quello che serve a un bosco.

Con un limite dichiarato: funziona sui set **ben separati**. Su uno fitto come
`periwinkle_plant` — sei piante su 1,2 m — gli ingombri si toccano e il set
collassa in un individuo solo, senza avviso.

**4. I kit modulari non hanno UV utilizzabili.** Il muro del kit ha **64
vertici in tutto**, e tutte le sue coordinate stanno in una finestrella
dell'atlante: da 0,22 a 0,59 in U, da 0,57 a 0,93 in V. Quell'atlante è una
**tavolozza** — ogni faccia campiona una cella di colore pieno — quindi
sostituire la texture con un materiale fotografato darebbe una macchia
uniforme. Da qui i materiali proiettati: la coordinata si ricava dalla
posizione. Come si riconosce il caso su un asset nuovo sta in `docs/03`,
sezione *I kit modulari non hanno UV*.

**5. Le piante da prato sono rosette, non ciuffi.** I tre fiori gialli del
catalogo sono alti 5-19 cm e più larghi che alti. Ne discende che la taglia
dichiarata in `gExtProp` non può venire dal modello stilizzato che sostituisce,
e che **`perAltezza` va messo a `false` per tutto ciò che cresce a terra**: la
celidonia tarata sull'altezza uscirebbe larga un metro e trenta.

---

## Le domande aperte

### A. La scelta delle varianti — **CHIUSA**

`src/meshgroup.c` riconosce gli individui dentro un modello, `LoadExtProps()` li
ricentra e fa un gruppo di lotti per variante, `PropVariantOf()` sceglie quale
disegnare dalla posizione. Il mondo cotto non è stato toccato. Design in
`docs/superpowers/specs/2026-09-05-varianti-prop-design.md`, funzionamento in
`docs/01`, sezione *Varianti*.

Restano fuori e restano YAGNI: i **pesi per variante** (una comune, tre rare) e
le varianti dichiarate a mano.

### B. Gli alberi — **RISOLTA DIVERSAMENTE**

Non si scrive nessuno spezzatore di mesh, e la ragione sta nel vincolo 2 qui
sopra: reindicizzare risolve gli indici, non il peso, e sbloccherebbe un asset
solo — `quiver_tree_01`, alto 2,72 m e di specie desertica.

Gli alberi restano stilizzati. Diventa realistico **il suolo su cui poggiano**:
cinque tipi di sottobosco, che il catalogo ha in abbondanza e della taglia
giusta. Design in `docs/superpowers/specs/2026-09-08-sottobosco-design.md`,
funzionamento in `docs/01`, sezione *Prop di dettaglio*.

Tre cose imparate qui:

- **il catalogo si misura senza scaricarlo**: i vertici per primitiva stanno
  negli accessori del `.gltf`, che pesa decine di KB. 145 modelli in pochi
  minuti, ed è così che si è scoperto che la vecchia conclusione era giusta per
  la ragione sbagliata;
- **aggiungere un tipo di prop costava sette punti**, non sei come credevo
  scrivendo il design: mi ero dimenticato il resoconto del `baker`, che stampava
  `(null)` per i cinque tipi nuovi — `printf("%s", NULL)` è comportamento
  indefinito, ed è la stessa trappola che `world.c` documenta da mesi;
- **niente asset vuol dire niente prop**, e la metà che conta è la collisione: il
  raggio sta nel mondo cotto, quindi un prop senza modello resterebbe solido e
  invisibile.

**Resta aperto:** se un giorno il catalogo avrà un albero grande a un peso
utilizzabile, la strada è già misurata. `quiver_tree_02`, alto 1,47 m, potrebbe
invece diventare un arbusto.

### C. I personaggi — aperta, e la più grossa

Non è un lavoro Poly Haven: giocatore e cinque NPC sono **riggati e animati**,
con un sistema `.attach` che aggancia le armi alle ossa, e servirebbe un'altra
fonte di modelli riggati con le animazioni da ri-targettare.

Due cose misurate che riguardano questa domanda: i personaggi sono **il costo
dominante del fotogramma** — togliendo `EntitiesDraw()` e `PlayerDraw()` dal
blocco d'ombra il passaggio scende da 3,3 a **0,26 ms** — e
`UpdateModelAnimation()` aggiorna posizioni e normali ma **non** le tangenti,
quindi un personaggio animato con normal map avrebbe le tangenti ferme alla
posa di riposo.

### D. La torre con il forte — **CHIUSA per la torre**, aperta per il resto

La torre di guardia è `tower_round`, il pezzo **12** dei venti di
`modular_fort_01`. Una riga di `BUILD_FILES` porta ora anche un indice, e il
caricamento riusa per intero la macchina delle varianti: separa i gruppi,
prende il k-esimo, lo ricentra e gli fa il lotto con le sole sue mesh. Design in
`docs/superpowers/specs/2026-09-08-mastio-del-forte-design.md`, funzionamento in
`docs/01`, sezione *Pezzi indicizzati*.

Misurato sul file vero: 45 primitive, 20 gruppi, **4.148** vertici nella
primitiva peggiore, **tre** materiali PBR — il documento diceva quattro, sono
tre — e 1,36 MB di geometria. In gioco: `x1.00 -> 13.5 m, semiampiezza 7.92 m,
a lotti`.

Due cose imparate qui, che valgono oltre la torre:

- **un indice è posizionale.** È riproducibile — l'ordinamento per `min.x` e poi
  `min.z` regge i due pareggi che il file contiene — ma se il catalogo ricuoce
  il file il 12 diventa un muro, in silenzio. Per questo la riga dichiara anche
  l'ingombro atteso, e fuori tolleranza si ripiega sui pezzi Kenney con un
  avviso che riporta i due ingombri;
- **una taglia nuova invalida il dato cotto.** Il raggio di collisione della
  torre sta nel mondo cotto, e ricuocerlo non basterebbe: i mondi già salvati
  resterebbero a 3,0 m. Il codice lo scavalca quando il mastio c'è.

**Resta aperto:** le mura, la porta `large_castle_door`, e il cortile in cui si
cammina — che vorrebbe collisione a segmenti con un varco, come le case.

### E. La cripta — **CHIUSA**

La cripta è un **tumulo**: quindici massi di `rock_moss_set_01` in un anello di
5,5 m con un varco da 2,99, e `gothic_statue` a fianco dell'ingresso. Ci si
entra, e dentro c'è il boss. Design in
`docs/superpowers/specs/2026-09-08-tumulo-della-cripta-design.md`, funzionamento
in `docs/01`, sezione *La cripta è una ricetta*.

Il documento diceva che la cripta «userebbe lo stesso interruttore dei materiali
proiettati, quindi il lavoro è già preparato». **Era sbagliato due volte:**

- quell'interruttore vive su `partBatch`/`buildProj`, cioè sul percorso degli
  edifici, mentre la cripta sta su `propVar`/`extProp`;
- e la proiezione qui sarebbe stata **obbligatoria**, non facoltativa:
  `graveyard/crypt.glb` ha **1.028 vertici e 33 coppie UV distinte**, tutte in
  una striscia dell'atlante — è una tavolozza, come `wall.glb`.

Il lavoro davvero preparato era un altro: la macchina delle varianti. Il
caricamento non è cambiato di una riga.

Due cose imparate qui:

- **le entità usano la stessa spinta del giocatore**, e il boss nasce esattamente
  a `cryptPos`, dove la distanza è zero e la spinta non ha una direzione. Prima
  nasceva dentro la lastra. Ora il centro del tumulo è vuoto apposta, e la spinta
  dal centro misura 0,00 m;
- **un asset inganna in due modi opposti**, e stanno in `docs/03`: il nome che
  promette massi e dà ciottoli, e l'ingombro giusto di un volume che è quasi
  tutto aria.

## Come si verifica che tutto regga

```bash
make            # Linux e Windows, zero avvisi
make prove      # le prove, esce non-zero se qualcosa non torna
make valida     # dati e mondo cotto
```

`make prove` compila ed esegue i **dieci** file in `tools/prove/`. Non c'è un
framework: una prova è un eseguibile che stampa una riga per controllo. Chi esce
77 non ha trovato un contesto OpenGL e viene contata come saltata. **Cinque**
non aprono nessuna finestra e girano ovunque: `scale`, che prova la collisione
con le scale dentro le case; `varianti`, che prova il raggruppamento delle mesh
e la scelta della variante; `mastio`, che prova la scelta di un pezzo per indice
e la guardia sull'ingombro; `tumulo`, che prova l'anello della cripta — che il
centro sia libero, che il varco sia uno solo e che l'anello sia chiuso altrove;
e `sottobosco`, che prova la tabella dei prop di dettaglio, i due cerchi del
tronco e il tetto per chunk. Geometria pura, senza GPU.

### Le prove si verificano sabotandole, e non è una formalità

**Una prova che non prende niente è peggio di nessuna prova.** In questo
progetto la regola ha già ripagato più volte, e vale la pena elencare i modi in
cui una prova può passare senza provare niente, perché si somigliano tutti:

- **il cubo che non si accorge.** Nella prova dell'instancing, un cubo non
  distingue una normale moltiplicata per la scala da una divisa, perché le sue
  normali sono versori sugli assi. Serve la sfera, e sta scritto nel file o
  qualcuno la toglierà credendola ridondante;
- **la misura sull'orientamento invece che sul motivo.** Nella prova della
  proiezione i contatori vedevano solo le discese, ma la camera specchia lo
  schermo rispetto al mondo: i ritorni della rampa erano salite, e il conto
  tornava zero;
- **il gradino nullo per costruzione.** Un controllo cercava la cucitura del
  tetto sulla riga dove le due proiezioni coincidono: sarebbe passato anche
  senza scrivere la miscela;
- **la texture a tinta unita.** Una normal map di un colore solo non distingue
  *dove* viene campionata, quindi non poteva accorgersi che il rilievo leggesse
  le UV sbagliate — che è esattamente il difetto contro cui esiste la
  proiezione.

- **la prova già in ordine.** Nel mastio il pareggio su `min.x` va provato con
  l'array **mescolato**: l'ordinamento è per selezione e a parità non scambia,
  quindi con i pezzi già in ordine di Z il criterio mancante darebbe lo stesso
  risultato di quello presente;
- **il pezzo storto che cade comunque.** Il pezzo sbagliato con cui si prova la
  guardia deve avere lo **stesso volume** di quello atteso — largo il doppio,
  alto la metà — o anche un confronto sul volume lo scarterebbe, e le due regole
  non si distinguono;
- **la tautologia sull'origine.** Controllare che il vertice ricentrato valga
  "l'originale meno `o`" passa con qualunque `o`, compresa una sbagliata. Va
  controllata la **proprietà**: dopo il ricentraggio il pezzo poggia a terra ed è
  centrato in XZ.

- **il varco senza l'anello.** Una prova che verifica *c'è un'apertura* senza
  verificare *che l'anello sia chiuso altrove* passa anche con un anello tutto
  buchi, purché il buco più largo sia uno. I due controlli vanno in coppia, e il
  secondo si scrive sull'indice del primo, non confrontando float;
- **il punto scelto dove la prova non morde.** Provare la spinta *sul centro
  esatto* di un prop passa anche senza aver scritto niente, perché a distanza
  zero la spinta non ha una direzione. Il punto va preso di lato.

Tutte queste sono state trovate **dai sabotaggi, non dalle revisioni**, su prove
che passavano.

### E alcune cose le prende solo un lettore

Due difetti di correttezza dello shader sono stati trovati leggendo il codice,
in revisione, e nessuna prova poteva vederli: in modo 2 la normal map perdeva lo
sfalsamento che l'albedo applicava, così sul tetto il rilievo non stava sopra le
sue scandole; e `cross(n, t)` invertiva la bitangente su **tre orientamenti di
faccia su sei**, per cui due pareti opposte della stessa casa illuminavano le
scanalature in versi opposti. Sono in `docs/01`, sezione *Materiali proiettati*.

### Misurare le prestazioni, e guardare il gioco

Non c'è un bersaglio del Makefile: si costruisce un binario strumentato da una
copia dei sorgenti **fuori dal repo**, si forza `GS_PLAY` in `GameInit()` **e**
in `GameNewWorld()`, si fa ruotare `g->player.yaw` per campionare tutte le
direzioni, e si gira 75 secondi. Tutti i numeri di prestazioni di questo
documento vengono da lì, sempre dallo stesso percorso.

Lo stesso binario serve per le verifiche visive, che nessuna prova può fare:
confrontare due inquadrature a pixel dice se un cambiamento ha toccato solo
quello che doveva. Due dettagli che fanno perdere tempo se non si sanno: al
menu i modelli sono **già caricati** — `GameInit()` chiama `GameNewWorld()` — e
l'uscita del registro su pipe è bufferizzata, quindi va rediretta su file o si
legge una schermata vuota.

---

## Documenti collegati

- `docs/01-architettura.md` — sezioni *Normal map*, *Instancing*, *Varianti*,
  *Pezzi indicizzati*, *La cripta è una ricetta*, *Materiali proiettati* e
  *Le prove*
- `docs/03-asset-pubblici.md` — il catalogo misurato, come si sceglie un asset,
  *I kit modulari non hanno UV*, *Le piante da prato sono rosette*, *Due modi in
  cui un asset inganna* e *Cosa non ha un equivalente texturizzato*
- `docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md` — il
  perché dell'instancing, con le misure che hanno cancellato la tappa LOD
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — la domanda A
- `docs/superpowers/specs/2026-09-06-materiali-triplanari-design.md` — i
  materiali proiettati, e in coda l'errore che conteneva: aveva dato per
  scontato che l'istanza fosse la casa, mentre è il pannello
- `docs/superpowers/specs/2026-09-08-mastio-del-forte-design.md` — la domanda D
  per la torre, e perché un indice non basta senza l'ingombro atteso
- `docs/superpowers/specs/2026-09-08-tumulo-della-cripta-design.md` — la domanda
  E, e perché una cripta non esiste nel catalogo
- i piani eseguiti stanno accanto ai design, in `docs/superpowers/plans/`
