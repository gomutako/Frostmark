# 06 — Stato del lavoro e prossimi passi

> **Se stai riprendendo il lavoro, parti da qui.** Questo file dice dove siamo
> arrivati, cosa è stato deciso e perché, e quali sono le domande ancora
> aperte. Gli altri documenti spiegano *come funziona* il gioco; questo dice
> *a che punto siamo*.

**Ultimo aggiornamento:** 8 settembre 2026, commit `6147704`.

---

## Dove siamo

Le prime due fasi del piano — normal map nello shader, poi instancing — sono
chiuse da tempo e non si toccano più. Quello che resta aperto è un obiettivo
solo: **sostituire ogni oggetto stilizzato del gioco con uno realistico.**

Non è un progetto solo. Guardandolo da vicino si divide in cinque pezzi con
blocchi diversi, e due — cripta e personaggi — non si risolvono affatto con
Poly Haven, perché quel catalogo è fatto di scansioni statiche:

| # | pezzo | stato |
|---|---|---|
| 1 | **erba** | **fatto** — `celandine_01`, cinque varianti |
| 2 | **alberi e pini** | **bloccato**: nessun albero CC0 sta sotto il tetto dei vertici. È la domanda **B** |
| 3 | **edifici** | **fatto** — materiali proiettati sui dieci pezzi modulari |
| 4 | **cripta** | aperto, caso singolo. È la domanda **E** |
| 5 | **personaggi** | aperto, il più grosso: serve un'altra fonte. È la domanda **C** |

### Cosa è realistico in gioco, oggi

- **massi**: `namaqualand_boulder_04`, ×0,87 → 2,2 m;
- **cespugli**: `shrub_02`, un set di quattro individui in un file — *4 mesh, 4
  varianti, ×0,85 → 1,4 m*, uno per prop;
- **erba**: `celandine_01`, *5 mesh, 5 varianti, ×2,18 → 0,6 m*. È raccoglibile,
  e il fiore giallo dell'asset fa da segnale al posto della pallina non tinta
  che aveva il modello procedurale;
- **edifici**: i dieci pezzi modulari di casa e torre portano quattro materiali
  fotogrammetrici — assiti scuri su muro, porta e finestra, scandole sul tetto,
  assi chiare su pavimento e scala, pietra sui quattro pezzi della torre.

### Cosa è ancora stilizzato

**Alberi, pini, cripta e personaggi.** I primi due per il vincolo 1 qui sotto,
la cripta perché una scansione del genere non esiste, i personaggi perché sono
riggati e animati e Poly Haven non ne ha nessuno.

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

**2. Nessun albero del catalogo CC0 sta sotto quel tetto.** Il più vicino,
`quiver_tree_01`, manca per 3.787 vertici; `island_tree_02` ne ha 625.401 in una
primitiva sola. Non è una questione di specie o di bioma: **non ce n'è uno
caricabile**.

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

### B. Spezzare le mesh oltre il tetto — aperta

È l'unico modo per usare gli alberi realistici: dividere una primitiva da
625.000 vertici in blocchi da 65.535, reindicizzando. C'è anche un problema di
peso, non solo di indici: `fir_tree_01` è 478 MB di sola geometria. Da valutare
se ne valga la pena, o se gli alberi stilizzati siano un prezzo accettabile con
tutto il resto realistico.

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

### D. La torre con il forte — aperta, e la più pronta

Cercando un kit modulare realistico si è trovato che **non esiste** per le case
di villaggio (vedi *Cosa non ha un equivalente texturizzato* in `docs/03`), ma
esiste per una fortezza: `modular_fort_01` è **45 mesh, 20 pezzi separati**,
tutti sotto il tetto dei vertici — massimo 4.148 — con 28.218 triangoli e
quattro materiali PBR veri. `large_castle_door` è una porta a scala umana, 2,01
× 2,96 × 0,30 m.

Sono bastioni: pezzi alti 8,5 m, lunghi fino a 14,8, uno da 15,84 × 13,50 ×
15,84. Accanto a case da 7,8 × 5,2 non è una torre, è un maniero, quindi andrà
scelto **quale** dei venti pezzi e non preso in blocco.

Una cosa emersa e utile: il raggruppamento delle varianti ha letto i 20 pezzi
del forte da solo. Il motore **sa già caricare** un kit modulare spedito in un
file unico; quello che non sa fare è scegliere quale pezzo va dove, perché non è
un sorteggio dalla posizione ma logica di costruzione — `DrawHouse()` colloca
cella per cella. Servirebbe indirizzare "il pezzo *k* del file *X*".

### E. La cripta — aperta, caso singolo

Un edificio del genere non esiste come scansione. O si compone da pezzi —
pietre, colonne, ruderi — o resta il kit. Userebbe lo stesso interruttore dei
materiali proiettati, quindi il lavoro è già preparato.

---

## Come si verifica che tutto regga

```bash
make            # Linux e Windows, zero avvisi
make prove      # le prove, esce non-zero se qualcosa non torna
make valida     # dati e mondo cotto
```

`make prove` compila ed esegue i **sette** file in `tools/prove/`. Non c'è un
framework: una prova è un eseguibile che stampa una riga per controllo. Chi esce
77 non ha trovato un contesto OpenGL e viene contata come saltata. Due non
aprono nessuna finestra e girano ovunque: `scale`, che prova la collisione con
le scale dentro le case, e `varianti`, che prova il raggruppamento delle mesh e
la scelta della variante — geometria pura, senza GPU.

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

Tutte e tre le ultime sono state trovate **dai sabotaggi, non dalle revisioni**,
su prove che passavano.

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
  *Materiali proiettati* e *Le prove*
- `docs/03-asset-pubblici.md` — il catalogo misurato, come si sceglie un asset,
  *I kit modulari non hanno UV*, *Le piante da prato sono rosette* e *Cosa non
  ha un equivalente texturizzato*
- `docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md` — il
  perché dell'instancing, con le misure che hanno cancellato la tappa LOD
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — la domanda A
- `docs/superpowers/specs/2026-09-06-materiali-triplanari-design.md` — i
  materiali proiettati, e in coda l'errore che conteneva: aveva dato per
  scontato che l'istanza fosse la casa, mentre è il pannello
- i piani eseguiti stanno accanto ai design, in `docs/superpowers/plans/`
