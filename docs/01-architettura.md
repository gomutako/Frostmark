# 01 — Architettura

## Il principio guida

Un gioco di questo tipo può facilmente diventare un groviglio. Frostmark segue
tre regole, e tutto il resto discende da lì.

**Regola 1 — Il mondo si interroga da un solo posto.**
`WorldHeight(world, x, z)` restituisce l'altezza del terreno in qualunque punto.
Chi la chiama non sa da dove viene, e questo è il punto: collisioni, mesh,
minimappa e villaggi passano tutti da lì.

- la collisione con il terreno è una riga: `if (pos.y <= WorldHeight(...))`;
- la mesh di un chunk si costruisce campionando la stessa funzione;
- la mappa del mondo si disegna campionando la stessa funzione su una griglia;
- non esistono desincronizzazioni tra "quello che vedo" e "dove sbatto".

Fino alla fase 3 del piano (`docs/05`) quella funzione *calcolava* l'altezza dal
seme: il mondo era una funzione, non un dato. Ora legge una griglia cotta una
volta da `tools/baker` e caricata da `assets/world/` — 2048 × 2048 quote a 2 m di
passo, 8,4 MB. La firma è la stessa, i chiamanti non sono cambiati, ma il mondo è
diventato **autoriale**: si può spostare un albero o spianare una radura, e la
modifica resta. Era il presupposto della fisica.

Il prezzo pagato, dichiarato: il salvataggio non è più un `unsigned int` che
rigenera tutto — il seme che contiene serve solo a riconoscere il mondo e a
rifiutare una partita fatta in un altro — e senza `assets/world/` il gioco non
parte.

**Regola 2 — Lo stato del gioco sta in una sola struttura.**
`Game` (in `game.h`) contiene mondo, giocatore, entità, proiettili, quest e stato
dell'interfaccia. Viene allocata `static` in `main.c` e passata per puntatore.
Niente variabili globali sparse, niente singleton, niente allocazioni dinamiche
nel ciclo di gioco.

**Regola 3 — L'interfaccia è una macchina a stati.**
`GameState` (`GS_MENU`, `GS_PLAY`, `GS_INVENTORY`, …) decide sia quale funzione
di aggiornamento gira, sia quale schermata viene disegnata. Aggiungere una
schermata significa: un valore nell'enum, un `case` in `GameUpdate`, un `case` in
`GameDraw`, una funzione in `ui.c`. Nient'altro.

## Il ciclo di gioco

```
main()
 └─ while (!WindowShouldClose())
     ├─ GameInput(g)                    una volta per fotogramma
     │   └─ switch (g->state)            tasti, mouse, menu, dialoghi
     ├─ GameSimulate(g, SIM_STEP)        a passo fisso, anche piu' volte
     │   └─ GS_PLAY → UpdatePlaying()
     │            ├─ PlayerUpdate()          movimento, gravità, stamina
     │            ├─ WorldUpdateStreaming()   carica/scarica chunk
     │            ├─ EntitiesPopulate()       spawn/despawn nemici
     │            ├─ EntitiesUpdate()         IA e attacchi
     │            ├─ ProjUpdate()             proiettili
     │            └─ input di combattimento/interazione
     ├─ GameUpdateCamera(g)              dopo i passi, una volta per fotogramma
     │                                    (dipende da yaw e pitch, che l'input
     │                                     aggiorna per fotogramma)
     └─ GameDraw(g)
         ├─ BeginMode3D → terreno, prop, entità, proiettili, acqua
         └─ 2D → marker, HUD, schermata attiva

`PlayerUpdate()` in discesa si **aggancia al terreno**: senza, a ogni passo il
suolo scende sotto i piedi, il giocatore resta in aria per un fotogramma e la
gravità lo riprende — un sobbalzo ritmico di pochi centimetri, e `onGround`
falso quasi sempre, quindi niente salto e niente parata. Ci si incolla solo per
il dislivello che un passo può giustificare (`STEP_DOWN_SLOPE` in `config.h`):
un salto nel vuoto resta una caduta, con il suo danno.

Il terreno non è più l'unica superficie calpestabile: negli edifici alti
`WorldSupportHeight()` restituisce il solaio del primo piano e la quota della
rampa delle scale, e `PlayerUpdate()` prende la più alta che stia sotto ai piedi
entro `STEP_UP_REACH` — un gradino. Più in alto di così non ci si arrampica,
quindi stando al piano terra il solaio di sopra non risucchia in alto, e
salendo la rampa ci si alza davvero. Misurato salendo: +1,37 m a metà rampa,
+2,60 in cima, che è l'altezza esatta di un piano.

La rampa però non è solo una quota su cui posare i piedi: è anche un volume.
Finché è stata solo una superficie la si attraversava, perché di fianco e da
sopra sta più in alto di un gradino e `WorldSupportHeight()` la scartava.
`ResolveHouse()` respinge quindi dalla cella della scala dove la rampa supera i
piedi di più di `STEP_UP_REACH`: il piede della rampa resta aperto — è da lì
che si sale — chi ci sta già sopra ha i piedi alla quota giusta e non viene
toccato, e dal piano di sopra la tromba resta libera per scendere. Disegno,
quota calpestabile e volume solido leggono la stessa `StairTop()`: se
divergessero si sbatterebbe contro un gradino che non si vede.

Il giocatore cammina sulla **superficie che vede**: `WorldIoHeight()` legge la
quota sui due triangoli del quadrato, con lo stesso taglio che usa
`BuildChunkMesh()`, non su una superficie bilineare. Le due differiscono di
pochi centimetri, ma la differenza cambia a ogni quadrato attraversato:
misurata correndo in diagonale a 9 m/s, 2,3 cm di ampiezza a quasi quattro
oscillazioni al secondo — un saltellio fine che si sentiva proprio correndo.
Lungo un asse della griglia le due superfici coincidono, ed è per questo che il
difetto spariva andando dritti a nord o a est.

### Luce e ombre

Prima non c'era luce: il terreno portava un'illuminazione **cotta** nei colori
dei vertici, calcolata una volta da una direzione fissa, e tutto il resto veniva
solo moltiplicato per la tinta del ciclo giorno/notte. Nessuna faccia era più
chiara di un'altra e niente proiettava ombra.

Ora `src/light.c` tiene un sole direzionale e una mappa di profondità vista dal
sole. Lo shader (`assets/shaders/scene.vs|fs`) è uno solo e serve terreno, prop
e personaggi: nel passaggio d'ombra scrive soltanto la profondità, nel disegno
vero somma ambiente e sole e legge l'ombra dalla mappa.

Tre decisioni che si vedono:

- **Le mappe sono due**, entrambe 2048 texel: una stretta attorno al giocatore
  (48 m di lato, 2,3 cm per texel) e una larga per il resto (120 m, 5,9 cm). Con
  una sola bisognava scegliere fra ombre nitide e ombre lontane: a 3,9 cm per
  texel, a tre metri dalla camera un texel copriva quasi sette pixel di schermo
  e i blocchi si vedevano. Le due passate costano 3,3 ms.
- **Il quadrato della mappa è spostato verso il sole**, non centrato sul
  giocatore: le ombre cadono dalla parte opposta al sole, quindi chi può
  oscurarti sta dalla parte del sole. Centrandolo sul giocatore, una torre a
  venti metri restava fuori e la sua ombra spariva.
- **La proiezione dev'essere quadrata quanto la mappa.** Accendendo il
  framebuffer a mano, `BeginMode3D()` prendeva l'aspetto dello *schermo*: la
  proiezione copriva 124 m in orizzontale contro 70 in verticale, schiacciati
  negli stessi texel, e le ombre uscivano a scaletta. `BeginTextureMode()` dice
  a raylib quanto è grande il bersaglio, e l'aspetto torna 1:1.
- **Il sole pesa quasi il doppio dell'ambiente** (0,85 contro 0,45). Con i due
  alla pari l'ombra toglieva un quarto della luce e non si vedeva.
- **La luce cotta nel terreno sparisce** quando lo shader c'è, altrimenti le
  colline sarebbero scure due volte. Senza `assets/shaders/` il gioco torna
  esattamente a com'era: l'assenza di un file non è un errore.

### Normal map

La normale del vertice descrive la forma grossa di un oggetto. Il rilievo fine
— la corteccia, la fuga fra due pietre — sta in una **normal map**, ed è metà di
ciò che fa sembrare realistico un asset. Lo shader la legge da `texture2`, che è
dove raylib lega `MATERIAL_MAP_NORMAL`; le mappe d'ombra vivono negli slot 10 e
11, apposta per non stare fra i piedi alle texture del materiale.

La mappa è espressa **in spazio tangente**, cioè relativa alla superficie: per
usarla serve la terna tangente/bitangente/normale. Tre trappole, tutte trovate
scrivendola:

- **Raylib lega `texture2` solo se il materiale ha davvero una normal map.**
  Senza, l'uniform resta a zero — cioè allo stesso slot dell'albedo — e lo
  shader leggerebbe il *colore* come rilievo: ogni asset senza normal map, cioè
  tutti quelli di oggi, si illuminerebbe a caso. Perciò `LightApplyToMaterial()`
  installa su chi non ce l'ha una normale **piatta**, un pixel `(128,128,255)`
  che vale `(0,0,1)` e significa "non piegare niente". Una copia per materiale e
  non una condivisa: `UnloadMaterial()` libera le texture delle mappe, e una
  texture sola liberata due volte è un guaio che si paga lontano da dove è stato
  commesso. Un pixel per materiale non si misura.
- **Quando la mesh non porta tangenti raylib passa `{0,0,0,0}`.** Un
  Gram-Schmidt su un vettore nullo dà NaN, quindi il fragment controlla prima di
  costruire la terna e in quel caso resta alla normale del vertice.
- **`GenMeshTangents()` di raylib 5.5 ignora `mesh->indices`**: legge i vertici
  a gruppi di tre come se la mesh non fosse indicizzata, e le mesh glTF lo sono
  quasi sempre. Su quelle costruirebbe triangoli che non esistono. `light.c` ha
  quindi il suo `BuildTangents()`, che segue gli indici e accumula sui vertici
  condivisi, così la tangente non si spezza sui bordi.

Il risultato è che **con gli asset di oggi non cambia un pixel** — misurato: un
piano grigio illuminato da un sole a `(0.6, 0.8, 0)` dà 113 sul canale rosso
prima e dopo — e un asset con normal map la usa da subito. Piegando la normale
di 30° verso il sole lo stesso piano passa a 129, e piegandola dall'altra parte
a 78: i valori che il conto prevede.

**Limite noto:** `UpdateModelAnimation()` aggiorna posizioni e normali ma **non**
le tangenti. Un personaggio animato con una normal map avrà quindi tangenti
ferme alla posa di riposo. Sui personaggi attuali, che una normal map non ce
l'hanno, non si vede; va risolto se ne arriverà uno che ce l'ha.

### Instancing

Prima i prop si disegnavano uno per volta: una `DrawModelEx()` per albero, per
tre passaggi — quello principale e le due cascate d'ombra. Misurato su un
percorso di 75 secondi, a visuale rotante: **~4,4 µs per chiamata** nel
passaggio principale, con mesh da un centinaio di triangoli. Non erano i
triangoli, erano le chiamate.

Ora `src/instancing.c` tiene un **lotto** per coppia (mesh, materiale). Il ciclo
di culling non è cambiato: dove chiamava `DrawProp()` ora chiama `InstAdd()`, e
a fine passaggio un `InstFlush()` disegna tutto insieme.

I numeri, stesso percorso:

| | prima | dopo |
|---|---|---|
| chiamate di disegno, picco | 1.766 | **144** |
| passaggio principale, peggiore | 6,0 ms | **2,0 ms** |
| chiamate nel passaggio d'ombra | 399 | **63** |
| scena, peggiore | 12,1 ms | **~6,7 ms** |

Quattro cose che non si ricostruiscono a memoria:

- **Il dato d'istanza è di 32 byte, non 100.** La soluzione ovvia manda una
  `mat4` per il modello più una `mat3` per le normali. Qui non serve: in questo
  gioco non esistono rotazioni libere, ogni prop è posizione, imbardata e scala.
  Il vertex shader ricostruisce entrambe le matrici da `vec4(pos, sin)` e
  `vec4(scala, cos)`. Seno e coseno si calcolano sulla CPU, una volta per
  istanza invece che una per vertice.
- **Il VAO è del lotto, non della mesh**, e non è pignoleria.
  `DrawMeshInstanced()` di raylib attacca gli attributi d'istanza al VAO della
  mesh e alla fine cancella il buffer **senza spegnere i divisor**: la stessa
  mesh, disegnata poi senza istanze, legge un buffer che non esiste più. Con tre
  passaggi per fotogramma il caso non è teorico. Il VAO separato punta ai VBO
  della mesh più il buffer d'istanza, e i due percorsi non si vedono nemmeno.
- **La normale si divide per la scala, la tangente si moltiplica.** La normale
  vuole l'inversa trasposta, che per scala più rotazione attorno a Y vuol dire
  dividere; la tangente giace sulla superficie e segue la matrice del modello.
  Con scala uniforme la differenza non si vede: si vede sulla falda del tetto,
  che è `(cella, cella·1,6, cella·nz)`.
- **Il tetto si svuota da solo**, con lo scarto delle facce posteriori spento.
  La coppia `rlDisableBackfaceCulling()` attorno al ciclo in `DrawHouse()` non
  basta più, perché lì ora si accoda soltanto e il disegno avviene dopo.
  Dimenticarlo dà soffitti trasparenti, e da fuori non si vede.

Due errori sono stati commessi e presi da una misura, e vale la pena ricordarli.

**Il passaggio d'ombra ha bisogno del suo giro di `Begin`/`Flush`.** Anche lui
passa da `PlacePart()`. Senza, le case accodate lì venivano buttate via
dall'`InstBegin` del passaggio principale, che gira dopo, e gli edifici
smettevano di proiettare ombra — mentre le chiamate del passaggio d'ombra
crollavano da 399 a 126 e sembrava un guadagno.

**E il costo del passaggio d'ombra non era dove sembrava.** Tolto l'84% delle
chiamate, il tempo è calato del 10%. Le ipotesi sono state misurate una per una:
non è il riempimento (2048 contro 1024 texel non cambia nulla), non sono le
chiamate, non è il terreno (togliendolo restava a 3,45 ms). Sono i
**personaggi**: il blocco d'ombra contiene anche `EntitiesDraw()` e
`PlayerDraw()`, animati e disegnati una volta per cascata, ed escludendoli il
passaggio scende a **0,26 ms**. La geometria del mondo nel passaggio d'ombra
costava ~0,7 ms e ora ne costa 0,26; tutto il resto sono sempre stati i
personaggi, che ora sono il costo dominante dell'intero fotogramma.

### Varianti

Metà del catalogo vegetale di Poly Haven non è un oggetto ma un **set**:
`shrub_02` sono quattro cespugli diversi in fila su sei metri. Caricato com'era,
dove andava un cespuglio ne comparivano quattro in miniatura. Ora il motore
riconosce gli individui dentro un modello e ne disegna **uno per prop**; è quel
che serve a un bosco, perché quattro forme girate a caso non si leggono come
quattro.

**Chi è un individuo.** `Mesh` di raylib 5.5 non porta il nome, quindi dopo
`LoadModel()` resta solo la geometria. La regola, in `src/meshgroup.c`, è una:
*due mesh sono lo stesso individuo se i loro ingombri XZ si toccano*, e il
contatto è transitivo. Nessuna tolleranza aggiunta, e su un caso vero non
serve: i quattro ingombri di `shrub_02`, con le rotazioni dei nodi già fuse nei
vertici, si succedono lungo X separati da 0,23, 0,27 e 0,29 m di vuoto. La
stessa regola tiene insieme le sei mesh di `nettle_plant`, che sono i materiali
di una pianta sola e stanno una dentro l'altra. I gruppi si ordinano per X
crescente, e a parità di X per Z: l'ordine decide quale indice tocca a quale
variante, e deve uscire uguale a ogni esecuzione, o lo stesso mondo salvato
mostrerebbe cespugli diversi.

**Ricentrare, una volta al caricamento.** raylib fonde le trasformazioni dei
nodi dentro i vertici, ed è la proprietà su cui poggia `InstModel`: tutte le
mesh condividono un'origine, quindi una sola trasformazione d'istanza vale per
tutte. È anche il motivo per cui i set non funzionavano — la seconda variante
porta cucito l'offset che la mette in fila. Per ogni gruppo si sottrae dai
vertici il centro XZ e il minimo Y (il centro in Y metterebbe mezza pianta
sottoterra) e si rifà l'upload con `UpdateMeshBuffer()`, prima di creare i
lotti. L'alternativa era un uniform per lotto e una sottrazione per vertice a
ogni fotogramma.

**La taglia è per variante, non per set.** Ogni gruppo si scala dal proprio
ingombro fino alla dimensione dichiarata in `gExtProp`: i quattro cespugli,
larghi 1,64, 1,28, 2,29 e 1,10 m, entrano a ×0,85, ×1,09, ×0,61 e ×1,28 e nel
mondo sono tutti larghi 1,4 m. Cambia la forma, non la misura — e il raggio di
collisione cotto nel file del mondo resta valido senza rigenerare niente.

**La scelta è una funzione della posizione**, non un dato: `PropVariantOf()`
usa `FmHash01` sulla posizione del prop con il sale 91, come `HouseShapeOf()`
fa con 77 per decidere se una casa è alta. Disegno, passaggio d'ombra e
collisione la richiamano e non possono divergere; il mondo cotto non cambia di
un byte e nel salvataggio non entra niente di nuovo. Il sale diverso serve a
non far correlare la variante di un cespuglio con la forma delle case.
(Del cespuglio, per la verità, l'ombra non si vede comunque:
`WorldDrawShadowCasters()` salta erba e cespugli, che nel bosco sono la
maggioranza dei prop e non proiettano niente che qualcuno noterebbe. La
coerenza fra ombra e disegno vale quindi per il masso e per la cripta.)

**Il ripiego non instanziato è cambiato di conseguenza.** Disegnava il modello
intero con `DrawModelEx()`; ricentrate, le varianti si accavallerebbero tutte
sull'origine. Ora è un giro di `DrawMesh()` sulle sole mesh del gruppo scelto,
con la matrice composta a mano e la soglia dell'alfa impostata a mano — chi
instanzia ce l'ha per lotto. Non è codice morto: gira quando `assets/shaders/`
manca, e nessuna prova lo copre perché nessuna prova gira senza shader. Si
verifica rinominando `assets/shaders/scene_inst.vs` e riavviando: confrontate a
pixel, le due strade danno la stessa immagine.

**Il ricentraggio vale anche per i modelli a un individuo solo**, non solo per
i set. Prima di questo ramo i vertici di un modello esterno restavano dove il
glTF li aveva cotti, e `DrawModelEx(model, pos, ...)` metteva quell'origine su
`pos`. Ora, che `MeshGroupSplit()` trovi un gruppo solo o si arrenda e
`LoadExtProps()` tratti il modello come un individuo (vedi sopra), l'origine
diventa comunque centro XZ + minimo Y. Misurato con un programma usa e getta
(fuori dal repo, riusa `MeshGroupSplit`/`MeshGroupOrigin` da `meshgroup.c`) su
tutti gli asset a un solo individuo attualmente su disco, moltiplicando lo
scostamento in unità di modello per la scala che gli dà `gExtProp`:
`tree.glb` e `pine.glb` si spostano di 2,2 e 1,9 cm in orizzontale e zero in
verticale; `graveyard/crypt.glb` non si sposta di niente (offset zero su ogni
asse — il file arriva già centrato e appoggiato). Sotto la soglia di
percezione per tutti e tre. Due asset invece si spostano in modo non
trascurabile: `rock.gltf` (`namaqualand_boulder_04`, il masso in uso) di 20,2
cm in orizzontale e 2,6 cm in verticale; `herb.glb` (l'erba curativa, 0,9 m in
gioco) di 23,4 cm in verticale — un quarto della sua altezza — e 11,7 cm in
orizzontale: prima di questo ramo l'erba affondava nel terreno per un quarto
della sua altezza, ora appoggia esattamente. È il motivo per cui tutti i prop
esterni ora poggiano a terra invece di galleggiare o affondare secondo come è
stato esportato l'asset.

### Pezzi indicizzati

Un kit modulare arriva in **un file solo**: `modular_fort_01` di Poly Haven
spedisce venti pezzi di fortezza in 45 primitive. Il raggruppamento delle
varianti li separa già da sé — è la stessa regola degli ingombri che toccano —
ma un edificio non è un sorteggio dalla posizione: `DrawHouse()` colloca cella
per cella, e serviva poter dire *quale* pezzo.

Da qui una riga di `BUILD_FILES` che porta anche un **indice**: `pezzo = -1`
vuol dire "tutto il file", ed è quello che dichiarano le dieci righe dei kit
Kenney; `pezzo = 12` è la torre tonda dentro la fortezza. Il caricamento fa la
stessa sequenza delle varianti — separa i gruppi, prende il k-esimo, lo ricentra
sull'origine e gli fa il lotto con `InstModelCreateSubset()` — e tiene una
**cache dei file**, perché un file da venti pezzi va aperto una volta sola.

**L'indice è riproducibile ma non stabile.** `MeshGroupSplit()` ordina per
`min.x` e, a parità, per `min.z`: sui venti pezzi del forte i due pareggi — a
10,00 e a 19,00 — si risolvono, quindi il gruppo 12 è `tower_round` a ogni
esecuzione e su ogni piattaforma. Ma se il catalogo ricuoce il file e riordina i
pezzi, il 12 diventa un muro e **non c'è nessun avviso**: è la stessa famiglia
di difetti di `boulder_01`, dove 66.122 triangoli sembravano innocui ed erano
67.042 vertici. Per questo la riga dichiara anche l'**ingombro atteso** e il
caricamento lo confronta con quello misurato, un quinto di tolleranza per lato:
fuori, si avvisa con i due ingombri e si ripiega sui pezzi Kenney.

**La taglia è metrica, non in celle.** `BUILD_CELL` vale 2,6 m e regge le case;
una torre da 15,84 no. Il pezzo indicizzato dichiara la sua taglia in metri,
come `gExtProp`, e la scala esce dall'ingombro misurato.

**Il ripiego non instanziato disegna solo le sue mesh.** Senza
`assets/shaders/`, `PlacePart()` cadeva su `DrawModelEx()`, che per un pezzo
dentro un file di venti disegnerebbe *tutta la fortezza* in un mucchio attorno
alla torre. Ora è una mesh per volta, come fa `DrawProp()` per le varianti.

**Il mastio è indipendente dal kit.** `hasKeep` non è `hasBuildParts`: i dieci
pezzi di Kenney sono "tutti o nessuno" perché mezza casa è peggio di una
scatola, mentre una torre Kenney è un edificio intero e giusto, solo stilizzato.
Se il forte manca, o non è il pezzo atteso, la torre resta quella di prima e non
è un errore.

**La collisione segue la taglia, da un numero solo.** Il raggio della spinta è
cotto nel mondo — 3,0 m — e ricuocerlo non basterebbe, perché i mondi già
salvati resterebbero a 3,0. Quando il mastio c'è, `TowerHalf()` e `TowerHigh()`
scavalcano il dato cotto, e le leggono **sia** la spinta del giocatore **sia** il
taglio della camera: se divergessero, la camera entrerebbe in un muro che il
giocatore non può attraversare. Ed essendo la torre tonda, il taglio passa da
`RayBox()` a `RayTrunk()` — il cilindro dei fusti degli alberi — perché una
scatola attorno a un tondo occluderebbe quattro angoli vuoti.

Un caso trovato misurando, non leggendo: sul **centro esatto** la spinta non
faceva niente. Il conto generico si arrende a distanza zero perché non ha una
direzione in cui spingere; su un prop da 3 m è un bersaglio stretto, sul mastio
— largo 15,84 e piantato al centro del villaggio — non lo è. Ora quel ramo
sceglie un asse: uscire da una parte qualunque è l'unica cosa migliore di
restare dentro la pietra.

#### La cripta è una ricetta

Lo stesso principio dei pezzi indicizzati, applicato a un prop invece che a un
edificio. La cripta non è un oggetto ma un **tumulo**: `CryptRing()` produce
quindici massi — posizione, forma, scala, rotazione, affondamento — **dalla
posizione del prop**, e la leggono in tre: disegno, spinta del giocatore e
taglio della camera. È la regola di `HouseShapeOf()`, per la stessa ragione.

La quota non sta nell'anello: la mette chi disegna, prendendola dal terreno
sotto ogni masso. Così il tumulo segue il pendio, e la ricetta resta geometria
pura — si prova senza mondo caricato e senza contesto grafico.

Sta in `PropBatchAdd()` e non in `DrawProp()` perché di lì passano **entrambi**
i passaggi, principale e ombra: scriverlo due volte vorrebbe dire due ricette da
tenere d'accordo. E la resa si decide **prima** di accodare qualunque cosa —
accodare metà massi e poi arrendersi farebbe ripiegare il chiamante, che li
ridisegnerebbe tutti, con i primi contati due volte.

Il caricamento non cambia: sono varianti, come `shrub_02`. `hasTumulo` distingue
un set da un modello singolo, così un asset a una variante torna a comportarsi
come sempre — quindici copie della stessa lastra in cerchio non sono un tumulo.

**Il centro resta vuoto**, ed è il punto: le entità usano la stessa spinta del
giocatore, e il boss nasce esattamente a `cryptPos`. Prima nasceva dentro la
lastra e ne usciva solo muovendosi; ora nasce nello spazio libero del tumulo.
Misurato: la spinta dal centro della cripta muove di **0,00 m**.

### Materiali proiettati

I pezzi modulari degli edifici vengono da due kit Kenney e **non hanno UV
utilizzabili**. Il numero che chiude la questione: `wall.glb` ha 64 vertici e
**4 sole coppie UV distinte**; `planks.glb`, il solaio, ne ha 192 e **2**. Non
sono coordinate, sono un indice di colore — ogni faccia campiona uno smalto
pieno dentro una tavolozza da 512x512. Ci si può mettere sopra qualunque
materiale fotografico: verrebbe un edificio a tinta unita.

Per questi pezzi la texture si ricava quindi dalla **posizione** invece che
dalle UV. La regola sta tutta in `assets/shaders/scene.fs`:

- **asse dominante**, non miscela a tre. I pezzi sono pannelli allineati agli
  assi: su una faccia piatta la scelta dell'asse è netta, e costa **un**
  prelievo di texture invece di tre. La V segue sempre la verticale
  dell'oggetto sulle facce laterali, ed è quello che tiene le file di assi
  parallele al muro.
- **in spazio oggetto**, non di mondo. Proiettando in coordinate di mondo una
  casa ruotata prenderebbe la venatura di traverso. Verificato in gioco: la
  casa a 88,6 gradi e quella a 191,2 gradi hanno le file di assi orizzontali
  allo stesso modo.
- **la miscela a tre solo sul tetto**, che è l'unico pezzo con la normale a 45
  gradi. Lì l'asse dominante oscillerebbe e a metà falda nascerebbe un gradino
  netto. I pesi sono il *quadrato* della normale: stringe la fascia in cui due
  proiezioni si sovrappongono, e quindi la sfocatura.

**La posizione locale è moltiplicata per la scala** prima di arrivare al
fragment, cioè arriva in metri. Senza, la texture si stirerebbe insieme al
pezzo, e c'è un pezzo scalato in modo non uniforme: la falda del tetto, che è
`(cella, cella·1,6, cella·nz)`. È lo stesso motivo per cui la normale
perturbata si **divide** per la scala tornando in mondo — moltiplicare darebbe
normali storte proprio sulla falda, dove nessuna prova le guarda perché
disegnano tutte a scala unitaria. Guardato sul modello vero: le scandole del
tetto hanno lo stesso passo delle assi del muro, e il rilievo non è storto.

**La normal map non usa le tangenti del vertice**, che i pezzi del kit non
hanno: la terna si costruisce dagli **assi della proiezione**, che sono gli
assi dell'oggetto. Chi ha UV vere continua a passare da `SurfaceNormal()` e
dalla tangente del `.glb`.

**La bitangente si dichiara, non si ricava da `cross(n, t)`.** Qui `t` e `bt`
non sono una terna generica: sono gli assi della proiezione, e la V ha una
direzione *nota per costruzione* — la verticale dell'oggetto sulle facce
laterali, la Z sulle facce orizzontali. Il prodotto vettore dà una terna
destrorsa, che coincide con quella direzione solo su **metà** degli
orientamenti: per l'asse dominante X con normale +X, per l'asse Z con normale
−Z e per l'asse Y con normale +Y il verso esce rovesciato. Dentro la stessa
casa la parete rivolta a +Z e quella rivolta a −Z illuminavano le stesse
scanalature in versi opposti — solchi su una, nervature sull'altra — e il piano
del solaio era fra le rovesciate. Entrambi gli assi si raddrizzano poi rispetto
alla normale (Gram-Schmidt), che serve solo alla falda del tetto, dove la
normale non sta su un asse. Il blocco 8 di `tools/prove/proiezione.c` lo
inchioda: la stessa mesh con la normale rovesciata, sotto lo stesso sole, deve
illuminarsi allo stesso modo.

**Il passaggio d'ombra non paga la proiezione.** Un frammento opaco esce da
`main()` subito, prima ancora del prelievo dell'albedo: il suo colore non lo
guarda nessuno, e sul tetto il modo 2 costava tre prelievi per frammento in
ogni cascata, buttati. Chi ha il ritaglio dell'alfa — le foglie — resta sulla
strada lunga, perché lì il colore decide la profondità e senza il `discard`
l'ombra di una fronda sarebbe un rettangolo.

**L'interruttore viaggia per lotto**, esattamente come `alphaCut`:
`InstProjection(lotto, modo, passo)` lo imposta prima del disegno e
`InstFlush()` lo rimette a zero prima di uscire, perché il lotto successivo
potrebbe avere UV vere. Non è teorico e non c'è prova che lo copra — le prove
disegnano un lotto solo per volta. Si misura in gioco: la stessa inquadratura
del villaggio, con e senza questo ramo, differisce **solo sui pixel degli
edifici**. Alberi, massi, personaggi, terreno e ombre restano identici, e in
un'inquadratura di solo bosco non c'è un pixel che cambi di più di 3 livelli su
921.600 — cioè il rumore dell'antialiasing, e nient'altro.

Che materiale va su che pezzo sta in `gBuildMat` in `world.c`. Il passo è per
**insieme** e non per pezzo: le assi di un muro e quelle di una finestra devono
avere lo stesso, o la finestra si stacca dalla parete.

| pezzi | materiale | passo | modo |
|---|---|---|---|
| muro, porta, finestra | `legno_scuro` | 2,0 m | asse dominante |
| tetto | `tetto_legno` | 1,5 m | miscela a tre |
| solaio, scala | `assito` | 2,0 m | asse dominante |
| i quattro pezzi della torre | `pietra` | 2,5 m | asse dominante |

Se un materiale sembra fuori scala si cambia **il passo**, non `BUILD_CELL`:
geometria e collisione dipendono dalla cella.

**Senza `assets/textures/` non succede niente di male.** Il materiale si monta
solo se il file c'è, e la granularità è per pezzo: rinominato il solo
`legno_scuro_diff.jpg`, muri, porte e finestre tornano alla tavolozza del kit e
al modo 0 mentre tetto, solai e torre restano proiettati — nessun avviso, il
gioco gira. La riga di registro lo dice a colpo d'occhio: *10 pezzi per gli
edifici modulari, 10 con materiale proiettato*, che senza quel file diventa
*7 con materiale proiettato*.

**Il ripiego non instanziato non è codice morto.** Quando manca
`assets/shaders/scene_inst.vs` i lotti non si creano e `PlacePart()` disegna
con `DrawModelEx()`; lì l'uniform va messo a mano con `LightSetProjection()`,
perché di lotto non ce n'è. Verificato rinominando quel file: la stessa
inquadratura di una parete differisce su **1.112 pixel su 921.600**, tutti sul
contorno di oggetti lontani e sullo zoccolo, e la superficie del muro è
identica a pixel.

**Lo sfalsamento si somma alla POSIZIONE, prima di proiettare.** Serve a due
cose insieme, e il modo in cui lo si applica decide se la seconda funziona:

1. differenziare le case fra loro — senza, trenta case avrebbero la venatura
   identica nello stesso punto del proprio corpo;
2. **tenere insieme i pannelli della stessa parete**, che è la parte che si
   sbagliava.

L'istanza qui **non è la casa: è il pannello.** `DrawHouse()` colloca muri,
porte, finestre e solai cella per cella, e ognuno è un'istanza a una posizione
diversa. La prima versione sommava la posizione dell'istanza alla coordinata di
texture *dopo* la proiezione, e per giunta senza distinguere l'asse: su una
parete la componente Z finiva sulla verticale, e il salto fra due pannelli
affiancati diventava una funzione dell'imbardata della casa. Misurato sullo
schermo, con passo 2,0 m e cella 2,6, correlando le due metà di un giunto:

| imbardata | scorrimento orizzontale | scorrimento verticale |
|---|---|---|
| 88,6 gradi (caso migliore) | 0,8 mm | 6,4 cm |
| 191,2 gradi | 1,10 m | 55 cm |

La correzione: il vertex shader passa al fragment la posizione dell'istanza
**riportata negli assi del pezzo** — ruotata all'indietro dell'imbardata — e il
fragment proietta `fragLocal + fragProjOffset` (`PosProiezione()`), una
posizione sola per l'albedo e per il rilievo. Sommarla prima di proiettare
rende la parete un **campo continuo**: due pannelli affiancati della stessa
parete differiscono solo lungo la direzione della parete, che è lo stesso asse
su cui corre la U, quindi il motivo prosegue attraverso il giunto invece di
ripartire. La rotazione non tocca la Y, quindi le file restano alla stessa
quota su tutti i pannelli dello stesso livello. La varietà fra case diverse
resta, perché case diverse stanno in posti diversi.

**Quantizzare lo sfalsamento al passo non era una correzione**, ed era scritto
qui: un multiplo esatto del passo, dopo la divisione per il passo, campiona
esattamente lo stesso texel: equivale a non avere sfalsamento affatto, cioè a
trenta case identiche.

**Limite noto e accettato:** agli spigoli, dove la rotazione locale del pezzo
cambia di 90 gradi, il motivo non prosegue. Su uno spigolo di casa è una
discontinuità attesa.

### Le prove

`make prove` compila ed esegue ogni file in `tools/prove/`. Non c'è un
framework: una prova è un eseguibile che stampa una riga per controllo ed esce
non-zero se qualcosa non torna. Includono il `.c` che provano, perché ciò che
vale la pena provare è quasi sempre `static`. Chi esce 77 non ha trovato un
contesto OpenGL e viene contata come saltata, non fallita.

Una prova che non prende niente è peggio di nessuna prova, quindi vanno
verificate anche loro. Quella dell'instancing confronta pixel a pixel lo stesso
oggetto disegnato nei due modi — zero pixel diversi su 25.600 — ed è stata
provata sabotando lo shader di proposito. Da cui una scoperta che sta scritta
nel file: invertendo il verso della rotazione la prendono entrambe le mesh, ma
moltiplicando la normale per la scala invece di dividerla **il cubo non se ne
accorge**, perché le sue normali sono versori sugli assi e `(1,0,0)` diviso o
moltiplicato per `(1, 2.5, 0.7)` resta `(1,0,0)`. Serve la sfera: con il solo
cubo la prova darebbe falsa sicurezza proprio sull'errore più facile da fare.

Giocatore ed entità non si attraversano: `EntitiesPushPlayer()` in `entity.c`
li separa come due cerchi sul piano, dopo che tutti si sono mossi — sta lì e
non in `PlayerUpdate()` perché il giocatore non conosce le entità. Lo
spostamento non si divide in parti uguali: la parte grossa la prende l'entità,
così camminando addosso a un popolano lo si scansa mentre un lupo che carica
non ti sposta di peso. Sui morti si cammina.

Le entità seguono la stessa fisica (`EntityFall()` in `entity.c`), che gira una
volta per entità a fine aggiornamento, qualunque cosa abbia deciso l'IA: prima
stava dentro il movimento, e un nemico fermo lasciato a mezz'aria non cadeva
mai. Non prendono danno da caduta: un lupo che si butta da una rupe darebbe
esperienza e bottino senza che nessuno lo abbia ucciso.
```

Nota su `dt`: viene limitato a 0,05 s in `main.c`. Senza questo limite, dopo una
pausa del sistema operativo il giocatore attraverserebbe il terreno in un frame.

## Streaming dei chunk

Il mondo è una griglia di 64 × 64 chunk da 64 m. Ne restano caricati al massimo
`(2·5+1)² = 121`, cioè un quadrato di 11 × 11 chunk attorno al giocatore.

`WorldUpdateStreaming()` fa due cose:

1. scarica i chunk oltre `VIEW_CHUNKS + 1` (isteresi: evita il caricamento
   ciclico quando si cammina lungo un confine);
2. carica i mancanti **al massimo 3 per frame** (`CHUNK_BUILDS_PER_FRAME`),
   procedendo per anelli concentrici dal giocatore verso l'esterno.

Costruire una mesh richiede 33 × 33 = 1.089 letture di `WorldHeight` più
altrettante di `WorldNormalAt` (che ne fa altre 4). Erano 5.445 valutazioni di
rumore, la parte più costosa del gioco; da quando il mondo è cotto sono 5.445
interpolazioni bilineari su una griglia in memoria. Restano distribuite su più
frame perché il caricamento sulla GPU non è gratis, non più perché il calcolo lo
sia.

I prop non si spargono più a ogni caricamento di chunk: si leggono da
`props.bin`, dove sono indicizzati per chunk, dieci byte per istanza.

## Le due visuali

`PlayerCamera()` costruisce la camera in prima o in terza persona partendo dagli
stessi dati: posizione, `yaw`, `pitch`. In soggettiva la camera sta negli occhi
(`PLAYER_EYE`) con un po' di *head bob*; in terza persona arretra di `camDist`
lungo la direzione della visuale, con uno scostamento in alto e a destra
(`CAM_RISE`, `CAM_SHOULDER`) perché altrimenti il personaggio coprirebbe
esattamente il punto inquadrato.

Due dettagli che è facile sbagliare:

**La mira non passa dalla camera.** Prima, mischia, magia e dialoghi usavano
`cam.target - cam.position`. In terza persona quel vettore parte da tre metri
dietro le spalle: il dardo di fuoco sarebbe nato dietro al giocatore e i
bersagli sarebbero risultati più lontani del vero. Ora esistono `PlayerEye()` e
`PlayerLookDir()`, e `EntityLookedAt()` prende origine e direzione invece di una
`Camera3D`. Il combattimento è quindi identico nelle due visuali, e la camera
resta un puro fatto di presentazione.

**Il terreno si mette in mezzo.** Se la camera arretrasse in linea retta,
salendo una collina finirebbe sotto la superficie. `PlayerCamera()` campiona
`WorldHeight()` in otto punti lungo l'arretramento e accorcia la distanza al
primo che sfonda, poi alza comunque la camera di `CAM_CLEARANCE` sul terreno.
Costa una manciata di `WorldHeight()` per frame, cioè niente: sono letture di una
griglia in memoria.

**Anche gli edifici si mettono in mezzo**, e lì il campionamento non basta: un
muro è sottile e fra due campioni ci passa. `WorldCameraClip()` taglia il
braccio della camera in modo esatto, contro le scatole degli edifici e i fusti
degli alberi — la chioma no, attraversare le foglie non dà fastidio e fermarsi a
ogni ramo darebbe una camera nervosa. Vedi la sezione *La camera non guarda mai
un muro* in `docs/03-asset-pubblici.md`. Quando la camera finisce comunque
addosso al collo, il personaggio si **dissolve** invece di sparire di colpo.

`charmodel.c` contiene il modello di personaggio animato, condiviso fra
giocatore e NPC: ricerca delle clip per nome, vista con le sole mesh skinnate
(altrimenti `UpdateModelAnimation()` di raylib crolla) e agganci di arma e scudo
alle ossa. La **posa non sta nel modello** ma in chi lo disegna, e per questo lo
stesso modello serve molti personaggi con animazioni diverse: si rideforma una
volta per personaggio dentro il ciclo di disegno.

Il corpo del giocatore (`PlayerDraw()`) è disegnato solo in terza persona. Se
`assets/models/player.glb` esiste si usa quel modello con le sue animazioni
(vedi `docs/03-asset-pubblici.md`), altrimenti le stesse primitive dei bipedi di
`entity.c`: capsula, testa, arma, più due gambe che oscillano su `bobPhase` — la
stessa fase che in soggettiva muove la testa.

![Terza persona](img/05-terza-persona.png)

## Illuminazione: com'era e com'è

Per molto tempo qui non c'era luce. Lo shader di default di raylib non ne
calcola, e invece di scriverne uno la luce diffusa veniva **cotta nei colori dei
vertici** quando la mesh nasceva:

```c
float diff = Vector3DotProduct(normal, SUN_DIR);
float lit  = 0.42f + 0.58f * fmaxf(diff, 0.0f);
```

Costava zero e restava leggibile, ma il sole era fisso, i prop e i personaggi
non avevano una faccia più chiara dell'altra, e niente proiettava ombra.

Adesso c'è uno shader vero (`assets/shaders/`, sezione *Luce e ombre* qui
sopra). La riga qui sotto resta nel codice per il caso in cui gli shader
manchino o non compilino: allora si torna esattamente al comportamento di prima,
perché l'assenza di un file non deve essere un errore.

## Costi e limiti noti

Le voci con un numero sono state misurate, non stimate.

| Aspetto | Scelta attuale | Costo o limite |
|---|---|---|
| Disegno terreno | 1 `DrawMesh` per chunk | ~121 draw call, 0,2 ms |
| Prop | 1–5 `DrawModelEx` ciascuno, distanza di disegno per tipo e chioma sola oltre i 120 m | 0,5–5 ms; era 25 ms prima del taglio per tipo |
| Edifici | composti dai pezzi del kit: 19 chiamate una casa bassa, 45 una alta | i villaggi sono nove edifici, non seimila come gli alberi |
| Ombre | due mappe 2048², una passata ciascuna | 3,3 ms; oltre 60 m dal giocatore non ce ne sono |
| Personaggi animati | skinning su CPU, ~0,06 ms per istanza | 6 draw call ciascuno; oltre 140 m tornano primitive |
| Culling | dot product sul forward, nessun frustum vero | prop dietro l'angolo disegnati inutilmente |
| Collisioni | cerchi 2D contro i prop, muri per le case, solai e rampe per gli edifici alti | nessuna collisione con il tetto: saltando sotto un solaio ci si passa attraverso |
| Entità | si separano fra loro e dal giocatore | non salgono le scale: la loro IA non sa che sopra c'è un piano |
| Nemici | 12 attivi attorno al giocatore | nessuna persistenza |
| Mouse | XInput2 sotto WSL, GLFW altrove | sotto WSLg il puntatore è assoluto: la visuale resta zoppa, si gioca con il `.exe` |

Sono tutti punti volutamente semplici: ognuno è un esercizio nel documento 04.
