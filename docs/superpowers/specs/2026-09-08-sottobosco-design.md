# Il sottobosco — design

**Data:** 2026-09-08
**Fase:** 6 del piano "asset realistici" (fasi 1–5 fatte)
**Domanda che riformula:** la B di `docs/06-stato-e-prossimi-passi.md`

## Perché la domanda B era mal posta

`docs/06` dice: *nessun albero CC0 sta sotto il tetto dei vertici*, e propone come
soluzione uno **spezzatore di mesh**. Misurando tutto il catalogo — 145 modelli
vegetali, il conteggio dei vertici letto dagli accessori dei `.gltf` senza
scaricare un solo `.bin` — vengono fuori due cose, e cambiano la domanda.

**Primo: 109 dei 145 stanno sotto il tetto.** L'affermazione «non ce n'è uno
caricabile» è falsa alla lettera. Ma quelli che ci stanno sono piccoli:
`quiver_tree_02`, che entra con 49.005 vertici e 16.530 di margine, è alto
**1,47 m**. `quiver_tree_01` è alto 2,72; `island_tree_02`, il primo che assomigli
a un albero da bosco, è alto 3,41 e ha **625.401 vertici in una primitiva**.

Il catalogo non ha alberi grandi ed economici: ha **alberelli costosissimi**.

**Secondo: lo spezzatore non servirebbe.** Reindicizzare risolve gli indici, non
il peso, e il peso è il vincolo vero:

| asset | vert/primitiva | altezza | peso |
|---|---|---|---|
| `quiver_tree_01` | 69.322 | 2,72 m | 8,9 MB |
| `island_tree_02` | 625.401 | 3,41 m | 46 MB |
| `jacaranda_tree` | 2.639.274 | — | 215 MB |
| `fir_tree_01` | 5.368.447 | — | 487 MB |
| `pine_tree_01` | 6.784.746 | — | **958 MB** |

Spezzare le mesh sbloccherebbe **un solo asset**, `quiver_tree_01`, alto due
metri e settanta e di una specie desertica. Gli altri restano centinaia di
megabyte per un albero.

Quindi la conclusione di `docs/06` era giusta nella sostanza e sbagliata nella
motivazione, e la strada che indicava non porta da nessuna parte.

## Cosa si fa invece

Gli alberi restano stilizzati. Diventa realistico **il suolo su cui poggiano** —
ceppi, tronchi caduti, radici affioranti, rami secchi, scaglie di corteccia — che
è dove il giocatore guarda camminando, e dove il catalogo ha roba economica e
della taglia giusta.

## Gli asset, misurati

Misurati il 2026-09-08 sui `.gltf` a 1k, ingombri dagli accessori con le
trasformazioni dei nodi applicate.

| asset | ingombro (m) | vert/primitiva | triangoli | gruppi | peso |
|---|---|---|---|---|---|
| `tree_stump_01` | 1,43 × 0,57 × 1,59 | 22.472 | 41.046 | 1 | 3,9 MB |
| `dead_tree_trunk_02` | 4,05 × 1,06 × 1,06 | 46.112 | 83.128 | 1 | 4,9 MB |
| `pine_roots` | 1,74 × 0,15 × 0,83 | 43.897 | 162.693 | **2** | 9,7 MB |
| `dry_branches_medium_01` | 0,32 × 0,34 × 1,30 | 4.252 | 16.803 | **3** | 3,3 MB |
| `bark_debris_01` | 0,19 × 0,10 × 0,61 | 39.748 | 193.406 | **4** | 8,1 MB |

Trenta megabyte in tutto, tutti sotto il tetto con margine. Tre dei cinque sono
**set**: il raggruppamento delle mesh li separa già, e danno nove varianti in
tutto senza una riga di lavoro in più.

## Una riga per prop di dettaglio

Aggiungere un tipo di prop oggi tocca **sei punti**: l'enum in `worldtypes.h`, la
riga di `gExtProp`, il `switch` di `PropMaxDist()`, il `switch` del ripiego
procedurale in `DrawProp()`, la lista di chi non proietta ombra, e l'emissione in
`worldgen.c`. Cinque tipi farebbero trenta modifiche in sei posti.

I cinque nuovi hanno però un comportamento **comune e diverso** da quello dei
sette esistenti, quindi diventano una tabella: un header condiviso
`src/propdefs.h` con una riga per tipo, letta sia da `world.c` per caricare,
disegnare e collidere, sia da `worldgen.c` per emettere.

```c
typedef struct {
    PropType type;
    const char *file;
    float   voluto;       /* taglia in metri              */
    bool    perAltezza;
    float   maxDist;      /* oltre, non si disegna        */
    float   raggio;       /* collisione: 0 = si attraversa */
    bool    ombra;        /* proietta ombra?              */
    float   frequenza;    /* quanto e' probabile fra i cinque */
} PropDetail;
```

`frequenza` e non `peso`: in questo documento "peso" sono i megabyte del file, e
due significati per la stessa parola sono un modo di sbagliare.

La tabella è un array, e `PropDetailOf(PropType)` torna la riga di un tipo o
**NULL** per i sette esistenti. È quel NULL che distingue un prop di dettaglio da
un albero, ed è come il disegno e la collisione sanno quale regola applicare.

Aggiungere un pezzo di sottobosco diventa **una riga**. I sette tipi esistenti
non si toccano: quello che funziona resta com'è.

### Perché non le alternative

Scriverli a mano come i sette esistenti segue il pattern alla lettera, ma sono
trenta modifiche e cinque primitive procedurali da inventare — di cui una per
oggetti grandi come una mano.

Un tipo solo, `PROP_DEADWOOD`, con cinque file dentro: `gExtProp` è un file per
tipo, quindi servirebbe una macchina nuova per "più file per un tipo", che è più
lavoro di quello che risparmia.

## Niente ripiego procedurale

I sette tipi esistenti hanno tutti una primitiva di riserva: senza `assets/` un
albero è un cilindro con una sfera sopra, e il gioco si gioca. Per il sottobosco
questa regola **non vale**, ed è una scelta dichiarata: una sfera schiacciata al
posto di una scaglia di corteccia è peggio del niente.

**Senza l'asset, il prop non esiste**: non si disegna e **non è solido**. Il
secondo pezzo è la parte importante — il raggio di collisione sta nel mondo
cotto, quindi senza questa regola il giocatore sbatterebbe contro tronchi
invisibili. Niente asset, niente prop.

Il gioco continua a funzionare senza `assets/`: la foresta è solo più spoglia,
com'era prima.

## Che cosa è solido

Solo il **tronco caduto**: 4,05 m di lunghezza e 1,06 di diametro sono un
ostacolo che si vede e si aggira, e aggirarlo dà alla foresta una texture di
percorso che oggi non ha.

Il **ceppo** no, e vale la pena dire perché: è alto 0,57 m, e il gioco non ha un
gradino — non si sale su niente. Un ostacolo all'altezza del ginocchio che ferma
di netto è fastidioso e sembra un difetto, non un ostacolo.

Radici, rami e corteccia stanno sotto i 35 cm: si calpestano.

## La seconda passata

Qui c'è il vincolo che ha deciso la forma della generazione. In foresta la catena
di `GenChunkProps()` riempie già **il 95,5% delle celle**: 80% alberi, 10%
cespugli, 2,5% erbe, 3% sassi, su una griglia 10×10 per chunk. Mettere il
sottobosco in quella catena vorrebbe dire **toglierlo agli alberi**.

Ma il sottobosco non è un'alternativa a un albero: è quello che sta *fra* gli
alberi. Quindi una **seconda passata**, con la sua griglia 8×8, i suoi sali
d'hash e la sua densità, indipendente dalla prima.

La griglia più larga — 8 m di cella contro 6,4 — è deliberata: il sottobosco deve
essere sparso, non tappezzare.

### Il tetto per chunk

`MAX_PROPS_PER_CHUNK` è **160**, e la prima passata ne emette al massimo 100 più
gli edifici. Una seconda passata da 64 celle al 35% aggiunge ~22 prop per chunk
di foresta, e 95 + 22 + 9 sta sotto il tetto — ma **va misurato**: `baker` conta
già i `fullChunks`, i chunk che toccano il tetto, e quel numero deve restare
zero. Se non lo resta, si alza `MAX_PROPS_PER_CHUNK` o si abbassa la densità, e
la scelta si fa sul numero.

### Le densità

Il sottobosco vive dove ci sono alberi da cui derivare:

| bioma | densità della seconda passata |
|---|---|
| foresta | 0,35 |
| collina | 0,18 |
| pianura | 0,08 |
| montagna | 0,05 |
| gli altri | 0 |

Quale dei cinque lo decide la `frequenza` dichiarata nella tabella:

| tipo | frequenza |
|---|---|
| corteccia | 0,30 |
| rami | 0,30 |
| radici | 0,17 |
| ceppo | 0,13 |
| tronco | **0,10** |

Somma 1,00. Il tronco è il più raro perché un albero caduto ogni due passi non è
una foresta, è un disastro — ed è anche l'unico solido, quindi la sua rarità è
anche quella degli ostacoli. Rami e corteccia sono i più comuni perché sono ciò
di cui un suolo di bosco è fatto davvero.

## Le distanze di disegno

Sono la difesa del fotogramma, e vengono dalla taglia:

| tipo | ingombro | distanza |
|---|---|---|
| tronco | 4,05 m | 140 m |
| ceppo | 1,43 m | 100 m |
| radici | 1,74 m | 60 m |
| rami | 1,30 m | 50 m |
| corteccia | 0,61 m | 40 m |

Una scaglia da 61 cm a 140 m è meno di un pixel e costa una riga d'istanza. Le
distanze stanno nella tabella e non in un `switch`, così cambiarle è cambiare un
numero.

Ombra: **solo tronco e ceppo**. Radici, rami e corteccia sono piatti a terra e
non proiettano niente che si veda, esattamente come erba e cespugli, che il
passaggio d'ombra salta già.

## Ricuocere il mondo

Il mondo cotto va rigenerato con `make mondo`, e questa volta è necessario: i
prop nuovi nascono lì.

Non rompe niente, ed è verificato: `assets/world/` **non è versionato** — è un
artefatto locale — e il salvataggio **non indicizza i prop**. `SaveData` in
`src/save.c` contiene giocatore, inventario, quest e contatori; lo stato `taken`
dei prop non c'è, e `worldio.c` lo azzera a ogni caricamento. Un mondo ricotto
con lo stesso seme accoglie un salvataggio esistente senza accorgersene.

Va però **detto nei documenti**: chi aggiorna il repo e non lancia `make mondo`
non vede il sottobosco e non capisce perché.

## Le prove

`propdefs.h` è una tabella e la generazione è aritmetica: si prova senza contesto
grafico, come `varianti`, `mastio` e `tumulo`.

- **la tabella è completa**: ogni tipo di dettaglio dichiarato nell'enum ha la
  sua riga, con file non nullo e taglia positiva. Una riga dimenticata darebbe un
  prop che non si disegna, e in silenzio;
- **la scelta pesata copre tutti e cinque** e rispetta le frequenze: su molte
  celle il tronco resta al 10% e la corteccia al 30%, entro una tolleranza
  dichiarata;
- **la seconda passata non sfonda il tetto**: sul chunk peggiore — foresta piena
  — la somma delle due passate più gli edifici sta sotto `MAX_PROPS_PER_CHUNK`;
- **la seconda passata è indipendente dalla prima**: cambiando i sali della
  vegetazione il sottobosco non si muove, e viceversa. Se condividessero un sale,
  i tronchi comparirebbero sempre accanto agli stessi alberi;
- **niente asset, niente solido**: un prop di dettaglio il cui modello non è
  caricato non spinge il giocatore.

### I sabotaggi

- **dare a tutti la stessa frequenza**: deve cadere il controllo sulle
  frequenze, o quel controllo sta solo contando che compaiano tutti;
- **usare lo stesso sale delle due passate**: deve cadere l'indipendenza;
- **alzare la densità a 1,0**: deve cadere il controllo sul tetto per chunk;
- **togliere una riga dalla tabella**: deve cadere il controllo di completezza;
- **rendere solido un prop senza modello**: deve cadere l'ultimo.

## Fuori ambito

- **Gli alberi realistici.** Chiusa: non esistono a una taglia e a un peso
  utilizzabili, e lo spezzatore di mesh non cambierebbe la risposta. La domanda B
  si chiude come *risolta diversamente*, non come *fatta*.
- **`quiver_tree_02` come arbusto.** Entra nel motore ed è alto 1,47 m, cioè un
  cespuglio: potrebbe sostituire `shrub_02` un giorno, ma è un'altra domanda.
- **Un gradino per salire sui ceppi.** Renderebbe i ceppi solidi senza renderli
  fastidiosi, ma è fisica del giocatore, non sottobosco.
- **`rock_moss_set_02`**, i sassi piccoli, e gli altri 104 vegetali sotto il
  tetto. Il catalogo misurato resta in `docs/03` per chi ne vorrà.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md` — la domanda B, e perché si riformula
- `docs/03-asset-pubblici.md` — *Due modi in cui un asset inganna*, e il catalogo
  misurato
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — le varianti, che
  i tre set usano senza modifiche
- `docs/superpowers/specs/2026-09-08-tumulo-della-cripta-design.md` — il prop che
  è una ricetta, e il raggio cotto scavalcato
