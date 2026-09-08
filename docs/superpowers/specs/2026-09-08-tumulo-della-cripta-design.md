# Il tumulo della cripta — design

**Data:** 2026-09-08
**Fase:** 5 del piano "asset realistici" (fasi 1–4 fatte)
**Domanda che chiude:** la E di `docs/06-stato-e-prossimi-passi.md`

## Perché

La cripta è l'obiettivo della quest principale: ci si arriva dopo ore, e quello
che si trova è una **lastra squadrata da 626 triangoli** con sopra la tavolozza
di Kenney. È l'ultimo oggetto stilizzato che il giocatore guarda da vicino con
l'attenzione alta, e l'unico dei quattro rimasti per cui non serve né spezzare
mesh né trovare personaggi riggati.

## Cosa non esiste, cercato prima di decidere

Interrogato il catalogo Poly Haven: **521 modelli**, nessuna cripta, tomba,
mausoleo o sepolcro. Cercato su nomi, categorie e tag — `ruin`, `tomb`, `grave`,
`crypt`, `temple`, `sarco`, `cemet`, `gothic`, `altar` — e i 93 candidati che
escono sono mobili vittoriani, statue decorative, rocce e il forte modulare.
`docs/06` lo diceva e la misura lo conferma: **un edificio del genere non esiste
come scansione.**

Due assi restano quindi: comporlo da pezzi, o tenere il modello del kit e dargli
un materiale vero.

## Il numero che chiude il secondo asse

`graveyard/crypt.glb` ha **1.028 vertici e 33 coppie UV distinte**, tutte dentro
una striscia dell'atlante: U da 0,094 a 0,969, V da **0,775 a 0,975**. È la
firma della tavolozza, la stessa di `wall.glb` (64 vertici, 4 coppie distinte).

**La cripta non ha UV utilizzabili.** Metterle sopra una pietra fotografata
darebbe una macchia uniforme, e servirebbe la proiezione — che oggi vive su
`partBatch`/`buildProj`, cioè sul percorso degli **edifici**, mentre la cripta
sta su `propVar`/`extProp`. `docs/06` diceva che «userebbe lo stesso interruttore
dei materiali proiettati, quindi il lavoro è già preparato»: **è sbagliato due
volte** — l'interruttore non è su quel percorso, e la proiezione qui sarebbe
obbligatoria e non facoltativa.

Resta il primo asse: comporre.

## Obiettivo

La cripta diventa un **tumulo**: un anello di massi muschiati con un varco da
cui si entra, una statua a fianco dell'ingresso, e dentro lo spazio in cui il
boss aspetta. Non un edificio, non un mucchio di sassi: un recinto sepolcrale.

In gioco oggi la cripta occupa 9,25 × 5,0 × 12,0 m — il tumulo sta in un
diametro simile, e il mondo cotto non si tocca.

## Gli asset, misurati

`rock_moss_set_01`, misurato il 2026-09-08 sul `.gltf` a 1k:

| gruppo | lato XZ maggiore | altezza | vertici |
|---|---|---|---|
| 0 | 3,37 | 1,47 | 6.078 |
| 1 | 2,13 | 1,77 | 5.691 |
| 2 | 3,26 | 1,26 | 6.192 |
| 3 | 3,00 | 1,21 | 8.538 |
| 4 | 2,76 | 1,26 | 4.748 |
| 5 | 2,11 | 1,10 | 2.741 |

**Sei gruppi puliti**, 63.127 triangoli in tutto, 8.538 vertici nella primitiva
peggiore — un ottavo del tetto — e 1,94 MB. Il raggruppamento li separa senza
tolleranze: non è un caso come `periwinkle_plant`, dove sei piante su 1,2 m si
toccano e collassano in una.

`gothic_statue`: **1,48 × 1,74 × 1,56 m**, 1 mesh, 23.314 vertici, un materiale
PBR, 3,98 MB. È a scala umana, ed è l'unico oggetto del catalogo che dica
"tomba" invece di "sasso".

Scartati e perché: `namaqualand_boulders_01` sono **ciottoli da 0,34 m** e
`namaqualand_rocks_01` è **ghiaia da 0,22 m**, malgrado i nomi. `rock_moss_set_02`
va bene (7 gruppi, 1,2–2,2 m) ma è il set dei sassi piccoli, e qui non serve.

## Il percorso: la cripta è già dove deve essere

Questa è la scoperta che rende il lavoro corto. `gExtProp[PROP_CRYPT]` carica già
un modello esterno, e `LoadExtProps()` sa già dividere un file in varianti,
ricentrarle e fare un gruppo di lotti per ognuna — è ciò che fa per `shrub_02`.

Quindi il caricamento **non cambia di una riga**: basta che `gExtProp` nomini
`rock_moss_set_01` invece di `crypt.glb`, e il motore trova sei varianti da solo.

Quello che manca è una cosa sola: **posare più di un'istanza.** Oggi `DrawProp()`
disegna una variante per prop; il tumulo ne vuole quindici.

### Perché non le alternative

Portare la cripta sul percorso degli edifici vorrebbe dire sei righe `BUILD_*`,
una per masso, ognuna con indice e ingombro atteso — sei dichiarazioni per una
cosa che il percorso degli ext prop fa già da sé, e una seconda copia del
raggruppamento.

Generalizzare `pezzo = -2` = "tutti i gruppi", dando ai `BUILD_*` la stessa
struttura a varianti, unificherebbe due percorsi che oggi duplicano la sequenza
separa–ricentra–sottoinsieme. È il lavoro giusto **il giorno che serve un
secondo caso**: oggi sarebbe una riscrittura in cerca di un uso.

## L'anello è una funzione, non un dato

`CryptRing()` produce i massi — posizione, variante, scala, rotazione,
affondamento — **dalla posizione del prop**, e la chiamano tutti e tre i
lettori: disegno, spinta del giocatore e taglio della camera.

È la stessa regola di `HouseShapeOf()` e per la stessa ragione: se il disegno e
la collisione calcolassero anelli diversi si sbatterebbe contro un masso che non
c'è, o si attraverserebbe pietra che si vede. Il mondo cotto resta identico —
la cripta è un prop nella stessa posizione, con lo stesso raggio scritto nel
file.

### I numeri dell'anello

| | |
|---|---|
| raggio | **5,5 m** |
| posti | **17**, a 2,02 m l'uno dall'altro |
| posti vuoti | **2, adiacenti** — è il varco |
| massi disegnati | **15** |
| passaggio libero al varco | **2,99 m** |
| vano interno | **4,10 m di raggio** (3,75 col masso più grande) |

Il passo di 2,02 m è **meno** del masso normalizzato (2,8 m di lato maggiore):
i massi si toccano, e l'anello si legge come un muro invece che come una fila di
sassi.

Due posti vuoti e non uno né tre, e il conto è questo: la corda fra i due massi
che fiancheggiano il varco è `2R·sin((v+1)·π/17)`, meno 2,8 di pietra. Con **un**
posto vuoto restano **1,17 m** — si passa di striscio e da fuori non si vede che
è un ingresso. Con **tre** ne restano **4,61**, e non è più un varco ma un lato
aperto. Con **due**: **2,99 m**, largo come una porta doppia.

Il vano da 4,10 m di raggio non è arbitrario: il boss ha `raggio = 0.75` in
`assets/data/entities.txt` e il giocatore 0,35, e lì dentro ci si combatte. I
tre revenant nascono a 12 m dal centro (`UpdateCrypt()` in `game.c`), quindi
restano **fuori** dal tumulo, che è dove devono stare: sono la guardia, non il
sepolto.

### Le taglie

I massi si normalizzano a `voluto = 2,8` sul **lato XZ maggiore**
(`perAltezza = false`, come i sassi già in uso: un masso si misura in larghezza).
Le scale escono fra 0,83 e 1,33, e le **altezze ci guadagnano varietà invece di
perderla** — da 1,22 a 2,32 m, perché normalizzare sulla larghezza lascia libera
l'altezza.

Sopra ci va, per masso e dal hash: una **scala** fra 0,85 e 1,25, una
**rotazione** libera attorno a Y, e un **affondamento** fra 0 e 0,3 m. Un tumulo
vero è assestato, non appoggiato.

## La collisione

Il raggio 5,0 è **cotto nel mondo** e ricuocerlo non basterebbe: i mondi già
salvati resterebbero a 5,0. Quando il tumulo c'è lo si scavalca, esattamente
come si è fatto per il mastio, e diventa **un cerchio per masso**: raggio =
semilato maggiore × scala del masso.

Il cerchio **circoscrive** il masso invece di inscriverlo, quindi blocca un po'
più della pietra vera. È la scelta voluta: preferibile sfiorare un bordo
invisibile che entrare dentro la roccia.

Il taglio della camera passa da `RayBox()` a **`RayTrunk()` per masso** — il
cilindro dei fusti degli alberi — perché quindici massi non sono una scatola.

**Il centro resta vuoto.** Non è un dettaglio: le entità usano lo stesso
`WorldResolveCollision()` del giocatore, e il boss nasce **esattamente** a
`cryptPos`, dove la distanza è zero e la spinta non ha una direzione in cui
agire. Oggi questo vuol dire che il boss nasce *dentro la lastra* e ne esce solo
quando comincia a muoversi. Con l'anello il problema sparisce da sé: al centro
non c'è pietra.

## La statua

Una riga sul percorso degli edifici: file intero (`pezzo = -1`), `uvVere = true`
perché ha UV vere e le sue mappe PBR, taglia dichiarata **1,74 m**.

Serve una piccola estensione: oggi una riga a file intero ignora `voluto`, e i
pezzi dei kit stanno sulla griglia di `BUILD_CELL`. Qui la taglia metrica va
usata, e la scala esce dall'ingombro del modello.

La statua sta **di fianco** al varco, non in mezzo: sfalsata di mezzo posto e
arretrata a 7,1 m dal centro, girata verso l'interno. In mezzo all'ingresso ci
si sbatterebbe entrando, ed è il punto in cui il giocatore corre.

Ha il suo cerchio di collisione, raggio 0,74.

### È la parte sacrificabile

Costa 3,98 MB e una riga di crediti, e serve a una cosa sola: che il tumulo si
legga come una tomba e non come un mucchio di sassi capitato lì. Senza, tutto il
resto funziona uguale — e va detto qui perché sia una scelta e non una scoperta.

## Il ripiego

Tre gradini, come sempre. Se `rock_moss_set_01` non c'è, `TrovaModello()` non
trova niente, `hasExtProp[PROP_CRYPT]` resta falso e si disegna la cripta
procedurale — la scatola con le quattro colonne che sta già in `DrawProp()`. Se
la statua non c'è, l'anello si disegna senza, e non è un errore.

Il modello del kit resta a un `mv` di distanza: `assets/models/graveyard/crypt.glb`
non si cancella, e ci si torna spostandolo in `assets/models/crypt.glb`, che è
il percorso che `gExtProp` dichiarerà.

## Le prove

Senza contesto grafico, come `mastio`, `varianti` e `scale`: `CryptRing()` è
geometria pura.

- **l'anello è deterministico e funzione della posizione**: la stessa cripta dà
  sempre gli stessi massi, e due posizioni diverse danno anelli diversi;
- **il centro è libero**: nessun cerchio di masso copre `cryptPos`, o il boss
  nasce nella roccia. È il controllo che vale di più, perché il difetto che
  previene non si vede finché non si arriva in fondo alla quest;
- **il varco è attraversabile**: esiste un'apertura larga più del giocatore;
- **l'anello è chiuso altrove**: nessuna *altra* fessura passabile — senza questo
  il controllo precedente passerebbe anche con un anello tutto buchi;
- **tutte e sei le forme compaiono** sui quindici posti;
- **senza il tumulo si torna al raggio cotto**, che è la promessa del ripiego.

### I sabotaggi

- **chiudere il varco** (zero posti vuoti): deve cadere "il varco è
  attraversabile";
- **allargare il passo** dei posti finché l'anello si apre: deve cadere "l'anello
  è chiuso altrove". È il sabotaggio che distingue *un* varco da *tanti*;
- **mettere un masso al centro**: deve cadere "il centro è libero";
- **far scegliere sempre la variante 0**: deve cadere "tutte e sei le forme
  compaiono";
- **far dipendere l'anello da un contatore invece che dalla posizione**: deve
  cadere il determinismo. È il difetto che farebbe divergere disegno e
  collisione, cioè il peggiore di tutti.

Un sabotaggio che non fa cadere niente vuol dire che la prova va riscritta.

## Fuori ambito

- **Un interno vero.** Il tumulo è un recinto: dentro c'è terreno ed erba, non
  una stanza scavata. Una discesa sottoterra vorrebbe il terreno bucato, che è
  un altro progetto.
- **La proiezione sul percorso degli ext prop.** Non serve più: i massi hanno UV
  vere. Resta da fare il giorno che un ext prop di kit vorrà un materiale vero.
- **Cambiare dove nascono i revenant.** Stanno a 12 m e restano fuori dal tumulo,
  che è giusto. `UpdateCrypt()` non si tocca.
- **Ricuocere il mondo.** Il raggio cotto resta 5,0 e il codice lo scavalca solo
  quando il tumulo c'è.
- **`rock_moss_set_02`**, i sassi piccoli ai piedi del tumulo. Sarebbero 1,94 MB
  per un dettaglio, e il set c'è se un giorno lo si vuole.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md` — la domanda E, e le due affermazioni che
  questa spec corregge
- `docs/03-asset-pubblici.md` — *I kit modulari non hanno UV: si riconosce
  dall'intervallo*, che è il controllo usato qui sulla cripta
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — le varianti, che
  questa usa senza modificarle
- `docs/superpowers/specs/2026-09-08-mastio-del-forte-design.md` — il raggio
  cotto scavalcato, e il numero solo con due usi
