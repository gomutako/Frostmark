# 06 — Stato del lavoro e prossimi passi

> **Se stai riprendendo il lavoro, parti da qui.** Questo file dice dove siamo
> arrivati, cosa è stato deciso e perché, e quali sono le domande ancora
> aperte. Gli altri documenti spiegano *come funziona* il gioco; questo dice
> *a che punto siamo*.

**Ultimo aggiornamento:** 16 settembre 2026.

> **L'obiettivo dichiarato è un clone di Skyrim, anche più semplice, ma
> realistico** — non qualcosa di "fumettoso". Detto dall'utente il 9 settembre
> 2026, e cambia il peso delle decisioni: finché l'obiettivo era "realistico
> dove conviene", tenere alberi e personaggi stilizzati era un compromesso
> difendibile. Ora è un **ripiego temporaneo da dichiarare**, non una risposta.
> Quando una strada porta a un risultato stilizzato — Kenney, KayKit,
> Quaternius — va scritto che è un ripiego.

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
| 2 | **alberi e pini** | **ripiego, ma ora con dei numeri e uno strumento**: un albero realistico da 4.600 vertici costa **+16%** sul passo principale, da 23.000 **+89%**. Sapling gira senza interfaccia e il conteggio è una manopola da 400 a 231.528 vertici — ma il budget si compra svuotando la chioma. Domande **B**, **bis**, **ter**, **quater**, **quinquies** |
| 3 | **edifici** | **fatto** — materiali proiettati sui dieci pezzi modulari |
| 4 | **torre** | **fatto** — `tower_round`, il pezzo 12 dei venti di `modular_fort_01` |
| 5 | **cripta** | **fatto** — un tumulo di massi con un varco. È la domanda **E** |
| 6 | **personaggi** | aperto, il più grosso: serve un'altra fonte. Il vincolo delle tangenti non c'è più — la domanda **F** è chiusa — quindi resta solo la domanda **C** |
| — | **le tangenti morte** — `BuildTangents()`, `vertexTangent`, `fragTangent` | **aperto**, ed è ora l'**unico** lavoro che la domanda **F** lascia dietro di sé: girano ancora, nessuno legge più ciò che producono. L'altro — la regressione fp32 — è chiuso il 16 settembre per la metà che dipendeva dalla terna; quel che ne resta non è suo, ha un nome e un conto, ed è diventato la domanda **G** |

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

### B. Gli alberi — ripiego riaperto dall'obiettivo, e ora **misurato**

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

**E con l'obiettivo dichiarato il 9 settembre, "risolta diversamente" diventa un
ripiego, non una risposta.** Un bosco di alberi stilizzati è l'80% di quello che
si vede a 260 m: è il singolo ostacolo più grande fra il gioco di oggi e un
Skyrim semplificato. Le tre strade erano **impostori** — pannelli con l'immagine
di un albero vero reso una volta sola, come li fanno i giochi grandi oltre una
certa distanza; **alberi generati** da uno strumento tipo Sapling o TreeGen di
Blender, a un conteggio scelto da noi invece che subìto dal catalogo;
**un'altra fonte** con licenza compatibile.

### B bis — quanto pesa un albero vero, misurato il 16 settembre 2026

Quel «nessuna delle tre è stata misurata» non vale più. Due lo sono.

**Il numero che ribalta la domanda: gli alberi di oggi costano già quanto un
impostore.** `tree.glb` ha **396 vertici** e `pine.glb` **444** — 226 e 254
triangoli. Un impostore ne ha otto. Il problema di B non è il costo: è
l'aspetto.

**Il denominatore.** Alberi e pini sono **121.567 prop su 204.386**, cioè il
**59,5%** del mondo. Nel punto più fitto ce ne sono 3.897 entro i 260 m di
distanza di disegno, e nel cono visivo da 102° ne cadono circa **1.108**: alla
taglia di oggi sono **0,44 M di vertici**, che su un passo principale da 2,2 ms
non è niente. Il budget è largo.

**La misura.** Banco in modalità costo, otto giri da 75 s, con tre baseline a
inizio, metà e fine — 2,244 / 2,249 / 2,280 ms, deriva **1,6%**. La macchina
delle scale tara ogni candidato a 6,5 m d'altezza, quindi l'ingombro verticale
sullo schermo resta lo stesso e cambia la ricchezza geometrica:

| albero | vert/albero | largh. a 6,5 m | passo principale | vs oggi |
|---|---|---|---|---|
| `tree.glb` (oggi) | 396 | 2,55 m | **2,244 ms** | — |
| `bush` | 4.649 | 7,69 m | **2,605 ms** | **+16%** |
| `statua` | 23.314 | 5,84 m | **4,252 ms** | **+89%** |
| `rock` | 30.165 | 8,63 m | 10,006 ms | +346% |

**Due candidati sono stati scartati, e il perché è la trappola di questa
misura.** `ceppo` (22.472 vertici, 10,811 ms) e `tronco` (46.112, 9,776 ms) sono
bassi e larghi in origine: `perAltezza` li scala sull'altezza nativa e li
trasforma in oggetti larghi **18 e 25 metri**. Non sono alberi più ricchi, sono
un'altra scena. Chi rifà lo sweep deve controllare la **larghezza risultante**,
non solo il conteggio dei vertici.

**La conclusione: gli alberi generati bastano, gli impostori non sono
obbligatori.** A quattro-cinquemila vertici per albero si sta dentro il +16%, ed
è il conteggio che Sapling o TreeGen danno se glielo si chiede. Tutti e quattro
i candidati portano una normal map vera, che l'albero KayKit non ha: quel +16%
non è solo geometria, è il passaggio da cartone ad asset fotogrammetrico nel suo
insieme.

**Tre limiti della misura, e vanno letti insieme ai numeri.**

- **È al punto di partenza, non al peggiore.** Da `1365,96 / 474,31` si vedono
  **1.608** alberi entro 260 m; il punto più fitto ne ha 3.897. Scalando
  linearmente — estrapolazione, non misura — il +16% varrebbe +0,87 ms e il
  +89% circa +4,9 ms;
- **la curva non è lineare, e c'è un salto non spiegato.** Fra `statua` e `rock`
  i vertici crescono di 1,29× e l'area frontale di 1,5×, ma il costo di
  **2,35×**. Né i vertici né la copertura da soli lo spiegano. È il primo posto
  dove guardare se un giorno il conto non torna;
- **il fogliame con ritaglio alfa non c'è dentro.** Nessuno dei quattro
  candidati ce l'ha. Un albero vero ha foglie su quadrati ritagliati:
  sovrascrittura, più una lettura di texture che si paga **anche nel passaggio
  d'ombra** (`docs/01`, sezione *Normal map*). Potrebbe dominare tutto il resto,
  e **resta da misurare**.

### B ter — cosa è chiuso delle altre due strade

**Gli impostori sono bloccati dove lo sono gli alberi generati.** Non sono
un'ottimizzazione — qui non c'è niente da ottimizzare — ma una tecnica di
qualità: disaccoppiano l'aspetto dal budget. Per generarne uno però serve
renderizzare un albero vero **una volta, offline**, e lì il blocco è confermato:
`pine_tree_01` ha **949 MB di sola geometria, identici a tutte e quattro le
risoluzioni** — la risoluzione cambia solo le texture (9, 33, 111, 346 MB). Il
numero del vincolo 2 era giusto, e non esiste una variante geometrica più
leggera da caricare.

**Un'altra fonte: un vicolo cieco in meno.** ambientCG, il candidato più ovvio
dopo Poly Haven, **non ha modelli 3D di piante**: nei primi 300 asset ci sono
258 materiali, 38 HDRI, 2 decal, 2 terreni e **zero** `3DModel`. Questa strada
ha ancora bisogno di un candidato con un nome.

**Blender era il blocco di entrambe queste strade, e non lo è più**: come si è
risolto sta in **B quater** qui sotto.

### B quater — Sapling gira, e il conteggio è una manopola (16 settembre 2026)

Il blocco è caduto. **Blender non era installato e `bpy` su pip non ha una build
per Python 3.14**, che è quello di questa macchina; la via d'uscita è stata il
tarball ufficiale estratto in `~/opt`, senza `sudo`, senza pacchetti di sistema,
e si disfa con un `rm -rf`.

**Sapling non è più nel pacchetto.** Dalla 4.2 gli addon di serie sono passati
alla piattaforma *extensions*, e nella 5.0.1 ne restano tredici. Sapling **non è
stato ripubblicato**: cercato su tutte le 1.447 estensioni del catalogo, non
c'è. Il suo sorgente GPL però è ancora online, sono due file più nove preset, e
**gira sulla 5.0 in `--background`**: `bpy.ops.curve.tree_add` risponde. Come si
riprende sta in testa a `tools/sapling_tree.py`.

Fra i nove preset ci sono `douglas_fir` e `small_pine`, cioè i due casi che
servono a questo mondo.

**Il conteggio dei vertici è un parametro**, su tre ordini di grandezza. Su
`douglas_fir`:

| configurazione | vertici | com'è |
|---|---|---|
| `levels 2` | **400** | gli stessi 396 dell'albero KayKit di oggi |
| `levels 3`, rami ridotti | 1.788 | solo struttura, niente chioma |
| ...più foglie rade | **7.624** | abete riconoscibile **ma spoglio** |
| rami fitti, `leaves 40` | 58.940 | più folto, ma la chioma legge come brina |
| default del preset | 231.528 | **oltre il tetto** del vincolo 1 |

**E qui la cosa che i numeri non dicevano, e che si è vista solo rendendo
l'albero:** il budget di vertici si compra **svuotando la chioma**. A 7.624
vertici la forma è giusta — tronco rastremato, rami a verticilli, silhouette da
conifera — e l'albero è spoglio come a febbraio. Un bosco di abeti nudi non è
più vicino a Skyrim di un bosco di cartone. **Guardare l'albero era necessario:
nessun conteggio lo avrebbe detto.**

La ragione è che Sapling spende geometria in **migliaia di foglioline
minuscole**, ed è lo strumento sbagliato per la densità.

### B quinquies — la strada che ne esce, e i due dettagli che la fanno fallire

I giochi risolvono la densità con **poche schede grandi e una texture di ciuffo
con canale alfa**. Quella texture è prendibile, ed è la scoperta che rende la
strada percorribile: le mappe del ciuffo di `pine_tree_01` si scaricano
**separatamente dalla geometria** — `twig_diff` 0,51 MB, `twig_alpha` 0,20 MB,
`twig_nor_gl` 0,80 MB, più `bark_diff` e `bark_nor_gl`, tutte CC0 a 1k. **I 949
MB che non possiamo caricare non servono.**

Due dettagli che, se non si sanno prima, fanno sembrare rotto il risultato:

- **il ritaglio alfa si accende dal FORMATO della texture**, non
  dall'`alphaMode` del glTF: `LightAlphaCutFor()` in `src/light.c:151-169`
  guarda se il formato ha un canale alfa. `twig_diff` e `twig_alpha` sono due
  file separati e vanno **uniti in un RGBA**, o il ritaglio resta spento. È lo
  stesso motivo per cui **il ritaglio di `bush` non è mai stato attivo in
  gioco**: il suo glTF dichiara `alphaMode: MASK`, ma la diffusa è un JPEG RGB
  senza alfa. Vale la pena saperlo anche fuori da qui;
- **l'albero esce con due mesh e zero materiali.** `tree` 4.010 vertici e
  `leaves` 4.908, entrambe sotto il tetto, ma senza materiali né immagini:
  Sapling non ne assegna. Sono due materiali da costruire, e solo il secondo
  vuole il ritaglio. Le due mesh si toccano in XZ, quindi la macchina delle
  varianti le riconosce come **un individuo solo** — che è giusto.

**Quello che resta da misurare, ed è l'ultimo buco:** il costo del ritaglio alfa
e della sovrascrittura di una chioma vera. Il banco non l'ha mai visto, perché
nessuno dei quattro candidati dello sweep aveva l'alfa attivo. Si misura col
primo albero texturizzato, non prima.
### C. I personaggi — aperta, la più grossa, e senza più prerequisiti tecnici

Non è un lavoro Poly Haven: giocatore e cinque NPC sono **riggati e animati**,
con un sistema `.attach` che aggancia le armi alle ossa, e servirebbe un'altra
fonte di modelli riggati con le animazioni da ri-targettare.

Due cose misurate che riguardano questa domanda. I personaggi sono **il costo
dominante del fotogramma**: togliendo `EntitiesDraw()` e `PlayerDraw()` dal
blocco d'ombra il passaggio scende da 3,3 a **0,26 ms**. E
`UpdateModelAnimation()` aggiorna posizioni e normali ma **non** le tangenti —
era la domanda **F**, cioè l'unico prerequisito tecnico di questa, e **non
blocca più**: la terna della normal map si costruisce dalle derivate di schermo,
che lavorano su posizioni già deformate dallo scheletro. Un personaggio scaricato
oggi porta in gioco il suo rilievo animato senza altro lavoro sullo shader.

Quello che resta da decidere qui è quindi solo **la fonte**, e l'ordine rispetto
alla domanda **B**.

#### Le fonti, cercate il 9 settembre 2026

**L'intersezione fra CC0, realistico e riggato è praticamente vuota.** Le prove:

- la categoria `rigged` di Poly Haven sono **15 asset e sono tutti prop** —
  sveglie, una morsa, un cannone, un orologio a pendolo. Zero umani, coerente
  con un catalogo di scansioni statiche;
- Kenney, Quaternius e KayKit sono CC0 su tutto ma **stilizzati per scelta
  editoriale**: i sei personaggi in gioco vengono da KayKit;
- su Sketchfab il filtro CC0 dà scansioni di persone, ma **statiche**: lo
  scheletro non c'è.

Tre strade praticabili, con la licenza verificata alla fonte:

**1. Mixamo.** Gratuito con un account Adobe, **royalty-free anche per giochi
commerciali**, personaggi e animazioni insieme più un auto-rigger. Il divieto è
ridistribuire i file grezzi come pacchetto di asset: qui non succede, perché
`assets/models/` è in `.gitignore` e il repository spedisce lo script, non i
file. Due limiti veri: i personaggi sono *semi*-realistici, non fotorealistici,
e **non è automatizzabile** — serve un account e non c'è API, quindi
`fetch_assets.sh` non può scaricarli, esattamente come già succede per
Quaternius.

**2. MakeHuman / MPFB.** L'unica strada per un umano **su misura e davvero
CC0**: l'addon è GPL, ma il modello esportato è CC0 e utilizzabile anche in un
gioco a sorgente chiusa. In cambio il rig e le animazioni restano da fare — la
Universal Animation Library di Quaternius ha 250+ clip CC0 su un rig umanoide
ri-targettabile.

**3. Scansione CC0 statica più auto-rig.** Il risultato più realistico e il
processo più fragile: la topologia da fotogrammetria si deforma male sulle
articolazioni.

Fonti: [Mixamo FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html) ·
[MakeHuman, licenza](https://static.makehumancommunity.org/about/license.html) ·
[MPFB, uso in giochi a sorgente chiusa](https://static.makehumancommunity.org/mpfb/faq/use_in_closed_source.html) ·
[Poly Haven API, categoria `rigged`](https://api.polyhaven.com/assets?t=models&c=rigged)

**Il consiglio, se si procede:** Mixamo è la strada pragmatica — licenza a
posto, animazioni incluse, ri-targeting risolto da loro. Ma l'ordine conta: un
umano realistico davanti a un albero di cartone peggiora l'insieme invece di
migliorarlo, quindi la domanda **B** e questa vanno guardate insieme.

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

### F. Le tangenti sulle mesh animate — **CHIUSA**

Era il prerequisito dei personaggi, ed è fatto: nel ramo `projMode == 0` di
`scene.fs`, `SurfaceNormal()` non legge più `fragTangent` e costruisce la terna
tangente/bitangente/normale dalle **derivate di schermo** — `dFdx` e `dFdy`
della posizione nel mondo e delle UV, il *cotangent frame*.

**Cosa bloccava.** `UpdateModelAnimation()` di raylib aggiorna posizioni e
normali ma **non le tangenti**: un personaggio con una normal map vera avrebbe
avuto il rilievo fermo alla posa di riposo, e si sarebbe visto sui volti. Le
derivate lavorano su `fragPosition`, che esce dal vertex shader dopo `matModel`,
cioè su posizioni già deformate dallo scheletro: non c'è nessuna tangente da
aggiornare, perché non c'è nessuna tangente. Il difetto non è riparato, è tolto
per costruzione. Ed era un prerequisito, non una riparazione: in gioco non c'era
niente da andare a cercare, perché nessun personaggio ha una normal map vera.

**Il costo, che era il rischio vero.** Le tangenti si pagavano una volta al
caricamento, le derivate si pagano per frammento. Soglia dichiarata e committata
*prima* di guardare il risultato: +5% sul passaggio principale. Misurato con sei
giri del banco da 75 secondi per copia, gli ultimi tre alternati perché la
macchina deriva verso l'alto man mano che si scalda — il passaggio principale
passa da **2,539 a 2,465 ms, cioè −2,9%**, col segno opposto a quello temuto. Il
merito non è tutto delle derivate: `fragTangent` è diventato un varying morto
che il compilatore può portare via da solo, quattro float per frammento, ma che
il −2,9% sia la somma dei due effetti è **plausibile, non verificato**: su
questa macchina l'attributo `vertexTangent` risulta ancora legato, la prova ne
stampa la location e vale 4. L'esperimento che scioglierebbe il dubbio è già
scritto nella spec; separare i due effetti resta il primo passo del lavoro che
resta aperto qui sotto.

**Il rumore sui triangoli sotto il pixel: esiste, ed è sessanta volte sotto il
tremolio da movimento.** Non è stato guardato, è stato **contato** — varianza
temporale su sessanta fotogrammi consecutivi, camera che avanza di un centimetro
a fotogramma sul nevaio a nord-est, con 51 prop Poly Haven con normal map vera
in campo. La differenza media fra fotogrammi consecutivi passa da 0,23933 a
0,24000 livelli, +0,25%; sui 114 pixel più sensibili — i bordi dei massi — la
varianza passa da 3,364 a 3,421, cioè **sei centesimi di livello** aggiunti a un
tremolio da movimento già sessanta volte più grande e a sua volta invisibile.
**Nessuna mitigazione è stata scritta**, e la decisione è motivata: lo
sfarfallio dei triangoli sub-pixel è il problema che risolvono LOD e impostori
— la domanda **B** — non quello della terna, e una soglia sulla distanza andrebbe
tarata su una geometria che è destinata a essere sostituita.

**Quello che resta scoperto.** Le UV degeneri **non** sono coperte: il design
del 9 settembre affermava il contrario, ed era falso. Dove le derivate delle UV
sono nulle il determinante è zero e la terna non esiste; il ripiego è lo stesso
di prima, la normale del vertice, su una condizione diversa. Il problema si è
spostato dal vertice al frammento.

**La regressione fp32 in campo vicino: misurata, e riparata a metà.** Le
derivate si prendevano su `fragPosition`, una posizione **assoluta in
metri-mondo** che arriva a `WORLD_SIZE` = 4096 (`src/config.h`). In fp32 un ulp
vale `x · 2⁻²³`: a `x ≈ 3000` sono circa `3,6·10⁻⁴ m`. Con `fovy = 70°` il
passo di mondo per pixel a distanza *d* è `2 · tan(35°) · d / righe`, cioè
`d · 1,3·10⁻³ m` su 1080 righe e `d · 1,9·10⁻³ m` sui 720 di `config.h`: **a un
metro dalla superficie il passo vale tre o quattro ulp**, a mezzo metro uno e
mezzo o due. E `dFdx()` è la differenza di due varying **già arrotondati**.

**Il difetto esisteva, ed è stato misurato prima di toccare il codice.** Banco
in modalità precisione (`tools/banco/README.md`): lo stesso masso a un metro,
una volta a `64,64` e una volta a `4032,4032`, con terreno, entità e ombre
spenti perché a quelle due posizioni sono differenze *vere* e coprirebbero
quella cercata. Sull'intero fotogramma la differenza media valeva **0,113
livelli** con un massimo di **67,8 su 255**; a mezzo metro 0,385 e 90,6. La
direzione conferma il conto: dimezzando la distanza il passo per pixel si
dimezza, l'ulp resta fermo, e l'errore relativo raddoppia.

**La cura, e la parte non ovvia.** Il varying è ora `fragPosRel`, relativa alla
camera, costruita in entrambi i vertex shader **senza mai formare il numero
grande** — la posizione locale in metri sommata alla differenza fra due
posizioni vicine. Due scorciatoie non funzionano, e stanno in `docs/01`:
sottrarre `viewPos` nel fragment, perché i bit sono già persi interpolando il
varying, e `world - viewPos` nel vertex, perché `world` è già arrotondato a
ulp(3000) e la sottrazione esatta conserva l'errore. Il rinominare è voluto: un
uso rimasto indietro non compila invece di sbagliare in silenzio. **Nessuna
riga di C** — `viewPos` era già impostata su entrambi i programmi, e un uniform
è del programma linkato, non dello stadio.

**Quanto ha guadagnato.** Sui pixel della superficie la differenza media passa
da **0,409 a 0,206** livelli a un metro, e i pixel che cambiano di almeno un
livello intero dall'**8,3% al 2,7%**. A mezzo metro da 0,754 a 0,311 e dal
18,1% al 3,3%. Il controllo che prende gli errori di segno — lo stesso masso
vicino all'origine, prima e dopo — dà 0,0067 contro un pavimento di rumore di
0,0022.

**Quello che resta non è la terna, ed è la domanda G.** Il conto non arriva a
zero, e tre misure dicono perché: il massimo sul bordo della sagoma è
**bit-identico** prima e dopo, la sagoma si sposta di 29 pixel **lo stesso
numero** prima e dopo, e il residuo non cala erodendo la maschera di sedici
pixel verso l'interno — quindi non è sbavatura di bordo. Design, misure e
tabelle in
`docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md`.

**Quello che resta da fare, ed è un lavoro suo.** `BuildTangents()`, l'attributo
`vertexTangent` e il varying `fragTangent` sono oggi codice morto: nessuno legge
più ciò che producono, e toglierli restituirebbe quattro float di varying su
tutta la scena. Non sono stati tolti qui, per due ragioni scritte nella spec —
avrebbero reso il numero del costo non attribuibile, e `normalmap.c` dichiara le
tangenti a mano proprio per essere lo strumento con cui questo lavoro si misura.
**La condizione che sblocca la rimozione: il percorso nuovo provato in gioco su
un personaggio con una normal map vera**, cioè dopo la domanda **C**. Tocca la
disposizione degli attributi in `instancing.c`, che è delicata, e il suo primo
passo è già scritto: rilanciare il banco tenendo vivo `fragTangent` con una
lettura inerte, per separare il costo delle derivate dal credito del varying
morto.

Design, misure e tabelle in
`docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md`; il
funzionamento sta in `docs/01`, sezione *Normal map*.

### G. La model-view si cancella a quattro chilometri — **aperta, e nuova**

Trovata misurando la F, il 16 settembre 2026, e non è la stessa cosa: la F
riguardava la terna della normal map, questa riguarda **dove finiscono i
vertici sullo schermo**.

`gl_Position = mvp * vec4(vertexPosition, 1.0)`, e `mvp = proj · view · model`.
All'angolo lontano della mappa la matrice di vista porta una traslazione di
−4032 e quella del modello una di +4032: il prodotto le **cancella in fp32**, e
la cancellazione lascia circa `ulp(4032)` = `4,9·10⁻⁴ m`. Il passo di mondo per
pixel a un metro vale `1,9·10⁻³ m` sui 720 di `config.h`, quindi lo scarto è
**circa un quarto di pixel**: la geometria rasterizzata scorre, e con lei
scorrono *tutte* le varying interpolate — le UV comprese, quindi il campione di
albedo e di normal map di **ogni** pixel della superficie, non solo dei bordi.

**Le prove che è questo e non la terna**, tutte e tre dal banco in modalità
precisione:

- il massimo della differenza **sul bordo** della sagoma è **bit-identico**
  prima e dopo la correzione della F: 49,5 livelli a un metro, 83,3 a mezzo
  metro. La F non lo tocca e non poteva toccarlo;
- la **sagoma si sposta**: 29 pixel discordi fra vicino e lontano, e sono 29
  sia prima sia dopo. Alla stessa posizione, zero;
- **non è sbavatura di bordo**: erodendo la maschera del masso di sedici pixel
  verso l'interno il residuo resta 0,188 contro un pavimento di rumore di
  0,002. E segue in parte il gradiente della texture — 0,115 nel quartile più
  piatto, 0,314 nel più inciso — che è la firma di uno scorrimento sub-pixel.

**La cura, non misurata:** costruire la model-view **relativa alla camera sulla
CPU**, cioè togliere la posizione della camera dalla traslazione del modello
prima di moltiplicare, invece di lasciare che due numeri da quattromila si
annullino dentro la matrice. È la strada che usano i giochi con mondi grandi.
Tocca tre punti, e nessuno è banale: `matModel` che raylib costruisce da sé per
`DrawMesh()`, la coppia `matView`/`matProjection` che `instancing.c` manda a
mano, e `instPosSin` che porta la posizione dell'istanza in coordinate assolute
dentro un buffer.

**Quanto vale la pena:** dopo la F il residuo misurato è 0,206 livelli medi
sulla superficie, con il 2,7% dei pixel che cambia di almeno un livello. È
sotto la soglia del visibile su un masso. Se morda su un volto a un metro —
che è il caso per cui tutto questo esiste — **non è stato misurato**, e non lo
si può misurare finché la domanda **C** non porta in gioco un personaggio con
una normal map vera. Questa domanda quindi **non blocca** niente: sta scritta
perché il conto c'è e il prossimo che misurerà un numero non nullo sappia già
dove guardare.

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

Il procedimento non è più solo a parole: `tools/banco/` versiona i due diff di
strumentazione e un `README.md` che spiega come costruire le due copie, cosa
ciascuna modifica fa e perché, le due modalità — costo e rumore — e il metodo
che rende le misure confrontabili fra loro invece che con l'assoluto.

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
- `docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md` — la domanda
  F: il cotangent frame, la misura del costo con la soglia dichiarata prima, e il
  conto dello sfarfallio sui triangoli sotto il pixel
- `docs/superpowers/plans/2026-09-09-tangenti-da-derivate.md` — il piano eseguito
  per quel lavoro
- `docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md` —
  la riparazione della regressione fp32, e in coda l'esito: dove la spec aveva
  torto, e come si è separata la terna dalla rasterizzazione
- `tools/banco/README.md` — il banco di misura, ora a tre modalità: costo,
  rumore e precisione
- `tools/sapling_tree.py` — come si genera un albero, con in testa come si
  rimette Sapling dentro un Blender che non ce l'ha più
- i piani eseguiti stanno accanto ai design, in `docs/superpowers/plans/`
