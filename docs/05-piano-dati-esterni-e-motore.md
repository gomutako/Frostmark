# 05 — Piano: dati esterni, mondo fisso, motore grafico e fisica

Piano di implementazione per tre cambiamenti richiesti insieme:

1. **tutti i dati fuori dal codice**, senza alcun valore predefinito interno;
2. **mondo fisso**, generato una volta da un seme e da lì in poi immutabile e
   modificabile, non ricalcolato a ogni avvio;
3. **motore grafico e fisica realistica** completi.

Il documento è pensato per essere seguito nell'ordine in cui è scritto: le fasi
sono ordinate in modo che nessuna vada rifatta a causa di quella successiva.

## Avanzamento

| Fase | Stato |
|---|---|
| 0 — Fondazioni | **fatta**: identificatori stabili (`dataid.h`), salvataggio v2, passo fisso con input e simulazione separati |
| 1 — Formato e caricatore | **fatta**: `dataparse.c` con diagnostica per riga, `gamedata.c`, `./frostmark --valida` |
| 2 — Migrazione dei dati | **quasi**: su file bilanciamento, oggetti, entità, quest, negozio, dicerie. Restano dialoghi, villaggi e testi dell'interfaccia (sotto il perché) |
| 3 — Mondo fisso | **prossima** |
| 4 — Fisica | da fare |
| 5 — Motore grafico | da fare |
| 6 — Editor | da fare |

In `src/` non resta nessun valore di gioco: statistiche dei nemici, ricompense,
prezzi, curva di esperienza, gravità e velocità stanno in `assets/data/`. Niente
ha un valore di ripiego: senza quei file il gioco non parte e dice cosa manca.

### Prossimo passo: fase 3, il mondo fisso

È il collo di bottiglia: la geometria di collisione statica si può precalcolare
solo su un mondo che non si rigenera, quindi **la fisica (fase 4) dipende da
questa**. Ed è misurata come la più economica fra quelle rimaste — cuocere il
mondo intero costa 1,1 s e produce 14,3 MB.

C'è anche una ragione per farla **prima** di completare la fase 2: villaggi e
organico degli NPC sono posizioni, e le posizioni diventano dati cotti qui.
Metterli su file adesso significherebbe riscriverli dopo.

Ordine di lavoro:

1. Formato: `manifest.txt` leggibile più `height.bin`, `biome.bin`, `props.bin` a
   chunk — progettato **per la scrittura**, così l'editor modifica un chunk senza
   riscrivere 14 MB.
2. `tools/baker`: riusa `noise.c`, `PlaceTowns()` e la generazione dei prop per
   emettere i quattro file, `spawns.txt` compreso (leggibile e correggibile).
3. `worldio.c`: carica altezze e biomi interi in memoria (12,6 MB), indicizza i
   prop per chunk, mantiene lo streaming attuale per le mesh.
4. `WorldHeight()` diventa un'interpolazione bilineare sulla griglia caricata:
   il codice esiste già, è quello del percorso `assets/heightmap.png`.
5. `noise.c` esce dall'eseguibile del gioco e resta solo negli strumenti.

Risoluzione: **2 metri**, la stessa della mesh (`CHUNK_SIZE / CHUNK_QUADS`). A 1
metro il file passerebbe a 33 MB e servirebbe infittire anche la mesh.

**Fatto quando**: il gioco si avvia solo da `assets/world/`, `noise.c` non è più
compilato nell'eseguibile, il mondo caricato coincide con quello generato
(altezze entro l'errore di quantizzazione, stessi biomi, stesso numero di prop) e
una modifica scritta a mano in `spawns.txt` si vede in gioco.

### Cosa resta della fase 2, e perché è rimasto

- **Dialoghi**: richiedono la grammatica di condizioni descritta più sotto
  (400-600 righe). Sono indipendenti da tutto: si possono fare in qualsiasi
  momento. Oggi `DialogueBuild()` confronta i tipi per identificatore, quindi la
  migrazione non toccherà altri file.
- **Villaggi e organico degli NPC**: da fare **dopo** la fase 3, per non
  progettare due volte il formato delle posizioni. L'organico è già scritto con
  identificatori (`"elder"`, `"guard"`) in `SpawnTownNPCs()`.
- **Testi dell'interfaccia**: 48 stringhe in `ui.c`. È localizzazione, un asse
  diverso: serve una convenzione di chiavi e, per validarla davvero, una seconda
  lingua. Estrarne una parte sarebbe peggio che non estrarne nessuna.

### Da fare comunque, e costa venti righe

Il validatore in integrazione continua. Senza valori di ripiego un dato rotto è
un gioco che non parte, quindi `./frostmark --valida` dovrebbe girare a ogni
push. Nel repository non c'è ancora nessuna CI.

---

## Premessa: tre promesse che cadono

Vale la pena metterle in chiaro una volta, perché sono scritte nel README e in
`docs/01`, e chi lavorerà al progetto se le aspetta.

**L'idea centrale del progetto cambia.** Oggi l'altezza del terreno è una
funzione pura `WorldHeight(seed, x, z)`: mesh, collisioni, minimappa, villaggi e
spawn interrogano tutti quella funzione, non esiste una mappa in memoria da
mantenere sincronizzata, e il salvataggio può contenere il solo seme. Con un
mondo fisso il terreno diventa un dato da caricare, indicizzare e tenere
coerente con ciò che gli editor ci scrivono sopra.

**Il gioco non parte più senza file esterni.** Oggi si compila e si gioca con
zero asset. Senza valori predefiniti interni, `assets/data/` diventa parte del
gioco quanto il codice: va versionata nel repository e validata, perché un
errore di battitura in un file non è più un dettaglio estetico ma un gioco che
non si avvia.

**La dimensione cresce di un ordine di grandezza.** `src/` sono oggi **4.354
righe**. A piano completato saranno 20-30.000, e non è più un progetto che si
legge in un pomeriggio. Questo è accettato: il valore didattico si sposta dalla
leggibilità immediata del codice alle potenzialità del progetto — un RPG open
world completo, in C, interamente open source, con motore grafico, fisica ed
editor propri e nessun dato chiuso nel codice.

---

## Misure di partenza

Numeri rilevati sul codice attuale, non stimati: sono la base su cui poggiano le
scelte che seguono.

| Grandezza | Valore | Come |
|---|---|---|
| Codice attuale | 4.354 righe in `src/` | `wc -l` |
| `WorldHeight()` | 0,26 µs per chiamata | 262.144 campionamenti cronometrati |
| Bake del terreno intero a 2 m | **1,1 s** su un core | 4.194.304 campioni × 0,26 µs |
| Campioni di altezza del mondo | 2048 × 2048 = 4.194.304 | 4096 m ÷ `VERT_STEP` (2 m) |
| Terreno cotto | 8,4 MB a 2 byte per campione | uint16, 1 cm di precisione |
| Biomi cotti | 4,2 MB a 1 byte | |
| Prop attesi nel mondo intero | **172.851** | distribuzione dei biomi × densità di `BuildChunkProps` |
| Prop cotti | 1,7 MB a 10 byte ciascuno | posizione quantizzata, tipo, scala, rotazione, flag |
| **Mondo cotto completo** | **~14,3 MB** non compresso | |
| Accoppiamento alle collisioni attuali | 3 chiamanti di `WorldResolveCollision`, 3 punti che incollano `pos.y` al terreno | `grep` |
| Testo dentro il codice | `quest.c` 55 stringhe, `ui.c` 48, `world.c` 33, `items.c` 28 | letterali ≥ 4 caratteri |
| Salvataggio attuale | 108 righe, indici numerici | `save.c` |

Distribuzione dei biomi misurata sul seme predefinito, usata per stimare i prop:

| Bioma | Quota del mondo | Densità di prop per cella |
|---|---|---|
| Oceano | 22,6 % | — |
| Pianura | 29,4 % | 0,315 |
| Foresta | 28,3 % | 0,955 |
| Spiaggia | 9,0 % | 0,205 |
| Colline | 7,4 % | 0,455 |
| Montagna | 2,1 % | 0,255 |
| Nevi perenni | 1,2 % | 0,155 |

**Conclusione operativa**: il mondo fisso è il punto meno rischioso del piano.
Cuocerlo costa un secondo, pesa quanto due modelli di personaggio, sta
comodamente nel repository e può essere caricato interamente in memoria. Il
terreno è campionato ogni 2 metri, non ogni metro, e questo dimezza quattro
volte il conto.

---

## Architettura di arrivo

Quattro strati, con una regola sola: **ogni strato conosce solo quelli sotto di
sé**.

```
tools/          baker (cuoce il mondo dal seme), validatore, editor
  │
data/           definizioni caricate una volta, immutabili
  │             oggetti, entità, quest, dialoghi, bilanciamento, testi
world/          mondo cotto: altezze, biomi, prop, punti di spawn
  │             caricamento, indice spaziale, streaming, scrittura (editor)
sim/            fisica a passo fisso, IA, quest, inventario
  │
render/         shader, ombre, culling, instancing, skinning su GPU
```

Cosa succede ai moduli attuali:

| Oggi | Domani |
|---|---|
| `noise.c` | esce dal runtime, diventa il cuore di `tools/baker` |
| `world.c` (807 righe) | si divide: `worldio.c` (carica e indicizza il mondo cotto), `worldmesh.c` (costruisce le mesh dei chunk), `worldedit.c` (scrittura per l'editor) |
| `items.c`, `quest.c` tabelle | scompaiono: restano i soli algoritmi, i dati vengono da `data/` |
| `player.c`, `entity.c` | il movimento passa al controller fisico; le statistiche arrivano da `data/` |
| `save.c` (108 righe) | riscritto: versionato, identificatori testuali, stato degli oggetti dinamici |
| `charmodel.c` | resta com'è, gli serve solo lo skinning su GPU (fase 5) |
| `config.h` | si svuota: le costanti di bilanciamento diventano `data/balance.txt`, restano i limiti di compilazione |

---

## Fase 0 — Fondazioni

**Perché prima di tutto**: sono decisioni che costano mezza giornata adesso e una
migrazione dei salvataggi dopo.

- **Identificatori testuali.** Oggi `ITEM_IRON_SWORD` è l'indice 2, e quel 2
  finisce nel salvataggio. Nei file gli oggetti si chiamano `iron_sword`; al
  caricamento il nome viene risolto in un indice runtime; il salvataggio
  memorizza un hash stabile a 32 bit del nome, non la posizione. Riordinare due
  righe di un file non deve cambiare il significato di una partita salvata.
- **Salvataggio versionato.** Intestazione con versione, sezioni con lunghezza
  dichiarata, rifiuto pulito delle versioni ignote con messaggio all'utente.
- **Passo fisso.** `main.c` passa a un accumulatore a 1/60 s con interpolazione
  in disegno. La fisica non funziona con `dt` variabile, e farlo ora evita di
  ritarare tutto dopo.
- **Le definizioni non contengono puntatori.** Lo stato runtime referenzia le
  definizioni per indice, mai per puntatore: è la condizione per poter
  ricaricare i dati a caldo.

**Righe**: 400-600 · **Tempo**: 3-5 giorni · **Rischio**: basso
**Fatto quando**: una partita salvata sopravvive al riordino di `items.txt`, e il
gioco gira a passo fisso con la stessa sensazione di prima.

---

## Fase 1 — Formato e caricatore

Il progetto usa solo raylib e la libreria standard C, e raylib non ha un parser
JSON. Il formato è quindi a sezioni, sulla riga di quello già usato per gli
agganci delle armi (`assets/models/player.attach`):

```ini
[oggetto iron_sword]
nome        = Spada di ferro
tipo        = arma
valore      = 120
potenza     = 14
descrizione = Lama d'ordinanza, affidabile e senza pretese.
```

Perché non JSON: un parser a sezioni sono 200-250 righe scritte a mano con
diagnostica per riga, i file restano leggibili nei diff, e un editor li emette
senza librerie. Perché non Lua: darebbe le condizioni gratis, ma sono 30.000
righe di dipendenza e la regola del progetto salta.

Il caricatore lavora **in due passate**: prima legge tutti i file e raccoglie le
definizioni, poi risolve i riferimenti fra loro. Così l'ordine dei file non
conta e i riferimenti mancanti si scoprono tutti insieme.

**Il validatore non è un extra.** Senza valori predefiniti interni, un dato
sbagliato è un gioco che non parte: serve un `frostmark --valida` che elenchi
tutti i problemi con file e riga, utilizzabile dall'editor e in integrazione
continua.

**Righe**: 500-700 · **Tempo**: 4-6 giorni · **Rischio**: basso
**Fatto quando**: `--valida` segnala per file e riga un riferimento inesistente,
un campo obbligatorio mancante e un valore fuori intervallo.

---

## Fase 2 — Migrazione dei dati

Ogni voce esce dal codice e le tabelle C vengono **cancellate**, non lasciate
come ripiego.

```
assets/data/
  items.txt       13 oggetti: nome, tipo, valore, potenza, descrizione
  entities.txt    8 tipi: vita, danno, velocità, portata, esperienza, oro,
                  bottino, raggio, altezza, comportamento, modello
  quests.txt      titolo, descrizione, obiettivo, bersaglio, ricompense,
                  chi la assegna, come avanza
  dialogues.txt   condizioni, testo, opzioni, azioni
  towns.txt       nomi dei villaggi e organico degli NPC
  shop.txt        assortimento del mercante
  rumors.txt      dicerie
  balance.txt     velocità, gravità, curva di esperienza, densità di spawn
  ui.it.txt       testi dell'interfaccia (apre alla traduzione)
```

Due punti meritano attenzione.

**I dialoghi non sono un albero, sono un generatore.** `DialogueBuild()` compone
il testo da tipo di NPC, stato delle quest e nome del villaggio; le opzioni
escono da un insieme chiuso di sei azioni, che è già a forma di dato. Per non
perdere quel comportamento serve una grammatica minima di condizioni e
variabili:

```ini
[dialogo guardia_lupi]
quando  = quest:lupi è offerta
voce    = $npc
testo   = I branchi sono scesi a valle e le greggi non sono più al sicuro.
opzione = Accetto l'incarico. -> accetta_quest lupi
opzione = Ci penserò.         -> chiudi
```

Da mettere in chiaro: **così il codice cresce.** Il generatore attuale ottiene
quel risultato in poche decine di righe; il valutatore di condizioni più il
parser ne costano 400-600. Il data-driven non si compra per scrivere meno
codice, si compra per iterare senza ricompilare e per poter dare il contenuto in
mano a un editor.

**Il comportamento resta codice.** Gli stati dell'IA, il disegno quadrupede del
lupo, l'aura del boss, il cablaggio dei contatori di quest: un tipo nuovo
definito da file scegle fra comportamenti esistenti (`comportamento =
bestia | umanoide | incantatore`), non ne inventa uno. Per quello servirebbe
scripting, che è fuori da questo piano.

**Righe**: +800-1.200, −400 di tabelle rimosse · **Tempo**: 1-2 settimane
**Rischio**: medio, per i dialoghi
**Fatto quando**: in `src/` non resta alcun valore di gioco né alcuna stringa
mostrata all'utente, e aggiungere un oggetto o una quest non richiede di
ricompilare.

---

## Fase 3 — Mondo fisso

Il seme diventa un **innesco, non un ingresso**: si usa una volta per cuocere il
mondo, e da quel momento il mondo cotto è la sorgente di verità, modificabile.
Rifare il bake è un'operazione da inizio progetto, non da ogni avvio.

**Il baker** (`tools/baker`) riusa `noise.c` e le funzioni di piazzamento
attuali per produrre, in un secondo:

```
assets/world/
  manifest.txt      seme d'origine, dimensioni, risoluzione, versione
  height.bin        2048 × 2048 uint16, 8,4 MB
  biome.bin         2048 × 2048 uint8, 4,2 MB
  props.bin         ~173.000 istanze indicizzate per chunk, 1,7 MB
  spawns.txt        villaggi, NPC, cripta: leggibile e modificabile a mano
```

Binario per altezze, biomi e prop, perché 12 MB di numeri in testo non hanno
senso; testo per il manifest e per i punti di spawn, che sono le cose che una
persona vuole aprire e correggere. Il formato dei chunk va scritto pensando alla
**scrittura**, non solo alla lettura: l'editor deve poter modificare un chunk
senza riscrivere 14 MB.

**Il runtime** carica altezze e biomi interi in memoria (12,6 MB: non vale la
pena streammarli) e mantiene lo streaming attuale per mesh e prop, che già
funziona a 121 chunk. `WorldHeight()` sopravvive come firma ma diventa
un'interpolazione bilineare sulla griglia caricata — esattamente ciò che già fa
quando si usa una heightmap esterna, quindi il codice esiste già.

Cosa si guadagna, oltre alla richiesta: il mondo diventa **autoriale** (si può
spostare un albero, spianare una radura, aggiungere un accampamento) e lo stato
degli oggetti diventa **persistibile**, che è il presupposto della fisica.

**Righe**: baker 600-900, runtime 800-1.200 · **Tempo**: 2-3 settimane
**Rischio**: medio · **Fatto quando**: il gioco si avvia solo da
`assets/world/`, `noise.c` non è più compilato nell'eseguibile, e una modifica
scritta in `spawns.txt` si vede in gioco.

---

## Fase 4 — Fisica

Dipende dalla fase 3: la geometria di collisione statica si precalcola dal mondo
cotto, e non avrebbe senso costruirla su un terreno che si rigenera.

Il mondo fisso rende due cose facili: il terreno diventa un **heightfield
collider** ricavato dalla griglia già caricata, e i prop hanno posizione e
identità stabili, quindi si possono precalcolare le forme statiche per chunk
(capsula per albero e pino, sfera schiacciata per il sasso, scatola per la casa,
cilindro per la torre, composto per la cripta: 8 tipi, ~150 righe di
descrizione).

Serve comunque, e va detto, un **controller cinematico** per il giocatore: anche
con un motore fisico sotto, il personaggio non si guida come un corpo rigido.
Capsula spazzata, gradino, limite di pendenza, aggancio al suolo, piattaforme
mobili: 500-1.000 righe a sé, in qualunque motore.

**Scelto: motore proprio**, in C, senza dipendenze. Cosa comporta, in ordine di
costruzione:

| Componente | Righe | Note |
|---|---|---|
| Forme e tensori d'inerzia (sfera, capsula, scatola, convesso) | 400-700 | |
| Broadphase | 300-600 | la griglia dei chunk è già un indice spaziale |
| Narrowphase analitico (sfera, capsula fra loro) | 400 | copre personaggi e proiettili |
| SAT scatola-scatola | 400-600 | copre case, torri, cripta |
| GJK/EPA per i convessi | 800-1.200 | rinviabile: serve solo per forme arbitrarie |
| Manifold persistenti e warm starting | 500-800 | senza questi le pile di oggetti vibrano |
| Solver a impulsi sequenziali, attrito, restituzione | 700-1.200 | |
| Integratore, isole, sleeping | 300-500 | |
| CCD almeno per i proiettili | 300-500 | altrimenti attraversano i muri |
| Controller cinematico del giocatore | 500-1.000 | serve comunque, in qualunque motore |
| Heightfield collider dal mondo cotto | 200-400 | |
| Viste di debug e banco di prova | 400 | non opzionali: senza, la taratura è a caso |

Per calibrare le righe: Box2D, che è **solo 2D**, sta attorno alle 10.000; Jolt e
Bullet sono nell'ordine delle 150-200.000. Un motore 3D minimo ma onesto —
sfere, capsule, scatole, GJK/EPA, manifold persistenti, impulsi sequenziali con
warm starting, isole e sleeping, CCD per i proiettili — difficilmente scende
sotto le 5.000.

**Una cosa da sapere prima di iniziare**: la fisica realistica non è un
miglioramento automatico. `GRAVITY 22.0` esiste **perché** 9,81 dà una
sensazione sbagliata: con l'accelerazione vera lo stesso salto durerebbe 1,5 s
e sembrerebbe di essere sulla Luna. Passare alla simulazione corretta obbliga a
ritarare altezze di salto, distanze di combattimento, velocità e camera. La
taratura è la voce di costo aperta di questa fase, più del codice.

**Fatto quando**: un sasso spinto giù da un pendio si ferma dove deve, un NPC
cade da un dirupo e prende danno, e il giocatore non resta impigliato negli
spigoli. Con un banco di prova ripetibile, non a occhio.

---

## Fase 5 — Motore grafico

Oggi non c'è alcuno shader: la luce è pre-calcolata nei colori dei vertici, per
evitare le differenze fra GLSL 100 e 330 e restare portabile sul web. Abbandonato
il target web, quel vincolo cade e si scrive una sola variante, GLSL 330.

| Componente | Righe | Note |
|---|---|---|
| Pipeline a shader, luce direzionale, materiali PBR o Phong con normal map | 700-1.000 | una sola variante, GLSL 330 |
| Ombre a shadow map a cascata | 500-800 | la voce più delicata: acne, peter-panning, cascate |
| Culling a frustum vero | 200-300 | oggi è un solo prodotto scalare; la griglia dei chunk è già l'indice |
| Instancing dei prop | 300-500 | il guadagno più grosso: oggi 1-5 `DrawModelEx` per prop, ~173.000 nel mondo |
| Skinning su GPU | 300-400 | `UpdateModelAnimationBones` e gli shader sono già nel submodule |
| Cielo, nebbia, acqua | 300-500 | |
| Post-processing: tonemap, FXAA | 200-300 | |
| Strumenti: viste di debug, contatori | 300 | senza questi le fasi 4 e 5 si tarano a caso |

**Righe**: 2.800-4.100 · **Tempo**: 4-6 settimane · **Rischio**: medio-alto
**Fatto quando**: le ombre sono stabili in movimento e il conto delle draw call
in una foresta sta sotto le poche centinaia.

---

## Fase 6 — Editor

Da fare per ultimo, ma i formati delle fasi 1 e 3 vanno progettati **adesso**
sapendo che ci sarà: scrittura parziale dei chunk, identificatori stabili,
validazione richiamabile.

Un editor interno al gioco riusa finestra, input, camera, mappa e menu già
esistenti, e ha il vantaggio di mostrare subito il risultato. Deve saper fare:
piazzare e spostare NPC e prop, modificare statistiche e quest, scrivere i file
di dati e i chunk del mondo, annullare e ripetere, validare prima di salvare.

**Righe**: 2.500-4.000 · **Tempo**: 4-8 settimane · **Rischio**: alto, per la
tendenza a crescere senza limiti
**Fatto quando**: una quest nuova con il suo NPC, il suo dialogo e il suo
bottino si crea senza toccare un file a mano e senza ricompilare.

---

## Riepilogo

| Fase | Righe nuove | Tempo | Dipende da |
|---|---|---|---|
| 0 — Fondazioni | 400-600 | 3-5 giorni | — |
| 1 — Formato e caricatore | 500-700 | 4-6 giorni | 0 |
| 2 — Migrazione dei dati | 800-1.200 | 1-2 settimane | 1 |
| 3 — Mondo fisso | 1.400-2.100 | 2-3 settimane | 0 |
| 4 — Fisica (motore proprio) | 5.500-9.000 | 6-10 settimane | 3 |
| 5 — Motore grafico | 2.800-4.100 | 4-6 settimane | — |
| 6 — Editor | 2.500-4.000 | 4-8 settimane | 1, 3 |

**Totale**: 14.000-21.000 righe nuove, 8-10 mesi di lavoro a tempo pieno per una
persona. Il codice finale è 4-6 volte quello attuale, e la fase 4 da sola vale
quanto tutte le altre insieme.

Le fasi 5 e 6 non hanno dipendenze stringenti dalle altre e possono procedere in
parallelo, se ci sono più persone.

---

## Decisioni prese

| | Scelta | Conseguenza |
|---|---|---|
| **Fisica** | **motore proprio** | 5.500-9.000 righe, 6-10 settimane; nessuna dipendenza; il rischio tecnico più alto del piano sta qui |
| **Linguaggio** | **C standard**, niente C++ | esclude Jolt, PhysX e Bullet; conferma la scelta sopra e vincola anche l'editor a raylib |
| **Mondo cotto** | **committato** nel repository | ~14 MB versionati; il mondo è autoriale e le modifiche dell'editor non vengono sovrascritte |
| **WebAssembly** | **abbandonato** | shader solo GLSL 330, nessun vincolo di memoria a 128 MB, `make web` e `make raylib-web` da rimuovere |
| **Valore didattico** | non più prioritario | si sposta dalla leggibilità del codice alle potenzialità del progetto e al suo essere interamente open source |

Le tre conseguenze operative da applicare subito:

- `.gitignore` deve tracciare `assets/data/` e `assets/world/`: da qui in avanti
  sono il gioco, non asset opzionali. Restano scaricati i soli modelli CC0 dei
  personaggi.
- I target `web` e `raylib-web` escono dal Makefile, e il README perde la
  sezione sulla build per il browser.
- Gli shader hanno una sola variante. Sparisce la voce «due varianti GLSL» dalla
  fase 5, e con essa la parte più noiosa di quella fase.

## Rischi e come tenerli sotto controllo

| Rischio | Perché | Contromisura |
|---|---|---|
| Dati rotti = gioco che non parte | è la conseguenza diretta del non avere ripieghi | validatore in integrazione continua, obbligatorio prima del commit |
| Churn del salvataggio | ogni fase tocca ciò che va persistito | versione dal primo giorno, hash testuali, test di andata e ritorno |
| Taratura della fisica senza fine | correttezza e sensazione sono obiettivi diversi | banco di prova ripetibile con scenari misurati, non giudizi a occhio |
| Instabilità del motore proprio | jitter, tunneling, pile che scivolano: sono i difetti classici, e si manifestano tardi | scenari di prova per ciascun difetto fin dal primo giorno, non alla fine |
| Perdita del determinismo | la virgola mobile non è riproducibile fra piattaforme | il terreno resta riproducibile perché è cotto; per i corpi dinamici la promessa si abbandona esplicitamente |
| L'editor cresce senza limiti | è la norma | definire il minimo utile — piazzare, modificare, validare, salvare — e fermarsi |
| Memoria complessiva | mondo cotto più sei personaggi animati | misurare a ogni fase; i personaggi già liberano le clip inutilizzate |

---

## Cosa non è in questo piano

Scripting per comportamenti nuovi definiti dai dati, audio (oggi assente del
tutto), rete, traduzione oltre l'estrazione dei testi, intelligenza artificiale
oltre gli stati attuali. Ognuna è un progetto a sé e va valutata dopo, sul
codice nuovo.
