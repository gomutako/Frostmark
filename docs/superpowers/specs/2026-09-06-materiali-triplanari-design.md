# Materiali proiettati sugli edifici — design

**Data:** 2026-09-06
**Fase:** sottoprogetto 3 del piano "tutti gli oggetti realistici"
**Precede:** la torre col forte (`modular_fort_01`), che è lavoro a sé

## Perché

Massi, cespugli ed erba sono asset fotogrammetrici. Le case e le torri no:
sono dieci pezzi modulari dei kit Kenney, e il giocatore ci passa più tempo
davanti che davanti a un masso.

Sostituirne la texture non funziona, e la misura dice perché. Il muro del kit
ha **64 vertici in tutto**, e tutte le sue UV stanno in una finestrella
dell'atlante: da 0,22 a 0,59 in U, da 0,57 a 0,93 in V, su un `colormap.png`
da 11 KB. Non è una mappatura della superficie: è una **tavolozza**, e ogni
faccia campiona una cella di colore pieno. Un muro di mattoni fotografato,
messo lì, darebbe un marrone uniforme.

Quindi o si danno UV a quella geometria, o si smette di usarle.

## Cosa non esiste, cercato prima di decidere

Prima di scrivere codice si è cercato un kit modulare realistico CC0 da
sostituire in blocco. **Non c'è.**

- Poly Haven ha un solo modulare medievale, `modular_fort_01`: 45 mesh, 20
  pezzi separati, tutti sotto il tetto dei vertici (massimo 4.148), texture PBR
  vere. Ma sono **bastioni** — pezzi alti 8,5 m, lunghi fino a 14,8, uno da
  15,84 × 13,50 × 15,84. Accanto a case da 7,8 × 5,2 non è una torre, è un
  maniero. Resta un buon asset per la torre, in un lavoro successivo.
- `large_castle_door` (2,01 × 2,96 × 0,30 m) e `modular_wooden_pier` sono utili
  ma isolati, non un kit.
- ambientCG è un catalogo di **materiali**: i suoi "modelli 3D" con nomi da
  muro — `Bricks104`, `PavingStones151` — sono texture.
- Kenney, KayKit e Quaternius sono stilizzati per scelta editoriale. È
  esattamente il look che si sta sostituendo.

Nota emersa dalla ricerca: il raggruppamento delle varianti ha letto da solo i
20 pezzi del forte. Il motore **sa già caricare** un kit modulare spedito in un
file unico; quello che non sa fare è scegliere quale pezzo va dove, perché non
è un sorteggio dalla posizione ma logica di costruzione — `DrawHouse()` colloca
cella per cella.

## La regola: asse dominante, in spazio oggetto

Si smette di usare le UV e si ricava la coordinata di texture dalla posizione.

**Asse dominante, non miscela a tre.** I pezzi del kit sono pannelli allineati
agli assi. Per una faccia piatta si guarda quale componente della normale è
maggiore in valore assoluto e si prendono le altre due coordinate come UV: **un
prelievo di texture**, come oggi. La miscela a tre proiezioni — il "triplanare"
classico — serve solo dove la normale è diagonale, e in questo kit c'è un caso
solo, il tetto.

**In spazio oggetto, non in coordinate di mondo.** Proiettando in mondo, una
casa ruotata di 40° prende la venatura di traverso, perché gli assi della
proiezione sono quelli della bussola. Il vertex shader però ha la posizione
locale del vertice — `vertexPosition`, in entrambi gli shader — prima della
trasformazione d'istanza. Si proietta su quella: le assi restano verticali e le
file parallele al muro comunque sia girata la casa.

La posizione locale si passa **già moltiplicata per la scala** dell'istanza. È
la differenza fra una texture in metri veri e una che si stira col pezzo: la
falda del tetto è scalata (cella, cella×1,6, cella×nz), e senza la scala le
scandole uscirebbero allungate di traverso proprio lì.

## Il tetto è il caso scomodo

Le falde del tetto a capanna hanno la normale a circa 45°: l'asse dominante
oscilla lungo la pendenza, e a metà falda nascerebbe una cucitura netta dove la
scelta cambia. Lì, e solo lì, si usa la **miscela a tre proiezioni** pesata
sulla normale — tre prelievi su un tipo di pezzo, che nel villaggio è una
manciata di lotti.

L'alternativa considerata e scartata: proiettare sulla falda usando la XZ
locale con un fattore di allungamento pari a 1/cos della pendenza. Costa un
prelievo invece di tre, ma è tarata su quel preciso modello di tetto e si rompe
in silenzio il giorno che l'asset cambia.

## Trenta case identiche

Con la stessa mappatura in spazio oggetto, ogni casa avrebbe la venatura
identica nello stesso punto: trenta copie della stessa parete. Il dato
d'istanza porta già la posizione, quindi la si usa per **sfalsare**.

> **Questa sezione diceva il falso, ed è l'origine di un difetto vero.**
> Diceva: «il dato d'istanza porta già la posizione nel mondo, quindi la si usa
> per sfalsare la coordinata di texture — una riga di shader, nessun dato
> nuovo», e dava per scontato che l'istanza fosse la casa. **L'istanza è il
> pannello.** Una casa è fatta di muri, porte, finestre e solai messi cella per
> cella, e ognuno è un'istanza a una posizione diversa: sfalsare per istanza
> non differenzia le case fra loro, differenzia i pannelli *dentro* la stessa
> casa. Sommando lo sfalsamento alla UV *dopo* la proiezione, per giunta senza
> distinguere l'asse, la componente Z della posizione finiva sulla verticale di
> una parete: 55 cm di scorrimento fra due pannelli affiancati di una casa
> ruotata di 191 gradi, misurati sullo schermo.

**La regola giusta: lo sfalsamento si ruota negli assi del pezzo e si somma
alla posizione, PRIMA di proiettare.** Il vertex shader passa al fragment la
posizione dell'istanza già riportata negli assi del pezzo — cioè ruotata
all'indietro dell'imbardata — e il fragment proietta `fragLocal +
fragProjOffset`, una posizione sola per l'albedo e per il rilievo
(`PosProiezione()` in `scene.fs`).

Così la parete diventa un **campo continuo**: due pannelli affiancati della
stessa parete differiscono solo lungo la direzione della parete, che è lo
stesso asse su cui corre la U, quindi il motivo prosegue attraverso il giunto
invece di ripartire. La rotazione non tocca la Y, quindi le file restano alla
stessa quota su tutti i pannelli dello stesso livello. La varietà fra case
diverse resta, perché case diverse stanno in posti diversi.

Lo sfalsamento resta una traslazione della posizione, non una rotazione della
texture: ruotare la texture romperebbe la verticalità delle assi, che è la
ragione per cui si proietta in spazio oggetto.

**Limite accettato:** agli spigoli, dove la rotazione locale del pezzo cambia
di 90 gradi, il motivo non prosegue. Su uno spigolo di casa è una discontinuità
attesa.

## La normal map

Senza normal map il legno scuro è una macchia scura: la resa di questo lavoro
sta lì, non nell'albedo. Ma la mappa è in spazio tangente, e la tangente del
vertice non serve più — la base va costruita dagli **assi della proiezione**.

Si perturba la normale in spazio oggetto e la si riporta in mondo con la stessa
regola che lo shader già usa per le normali: `RuotaY(n / scala)`. Perché il
fragment possa farlo, il vertex shader gli passa cinque float in più oltre alla
posizione e alla normale locali: seno e coseno dell'imbardata, e l'inverso
della scala.

Nel percorso non instanziato la scala sta dentro `matModel` e si ricava dalla
lunghezza delle sue prime tre colonne. È il percorso che gira quando manca
`assets/shaders/`, e non è codice morto.

## L'interruttore

Vale **per lotto**, e sta spento di default. Il precedente è `alphaCut`:
`InstCreate()` lo calcola dal materiale e lo carica come uniform prima di
disegnare, rimettendolo a zero dopo, perché un lotto non deve lasciare acceso
qualcosa per il successivo.

Qui il dato ha **tre stati**, non due, perché il tetto ha bisogno del terzo:

| stato | chi lo usa | costo |
|---|---|---|
| spento | massi, cespugli, erba, personaggi — hanno UV vere | un prelievo, per UV |
| asse dominante | muri, porte, finestre, pavimenti, scale, torre | un prelievo |
| miscela a tre | il tetto, e solo lui | tre prelievi |

Insieme allo stato viaggiano i **metri per ripetizione** della texture. Chi non
accende niente continua a campionare esattamente come adesso.

## La tabella dei materiali

Quattro insiemi per dieci pezzi:

| insieme | pezzi | perché |
|---|---|---|
| assiti scuri catramati | muro, porta, finestra | è il look scelto: stavkirke |
| scandole di legno | tetto | la miscela a tre, l'unico caso |
| assi chiare | pavimento, scala | dentro casa il legno è grezzo, non catramato |
| pietra | quattro pezzi della torre | una torre di legno non regge, e sarà lei a ricevere il forte |

I metri per ripetizione sono per insieme, non per pezzo: le assi di un muro e
quelle di una finestra devono avere lo stesso passo o la finestra si stacca.

## Da dove vengono le texture

Poly Haven ne ha in abbondanza, ma `fetch_assets.sh` oggi sa scaricare
**modelli**: `polyhaven_get.py` cerca il glTF e i file che gli stanno accanto.
Serve un ramo nuovo per i materiali, che prenda albedo e normal map a una
risoluzione dichiarata e le metta in `assets/textures/`.

Le due mappe si montano su `MATERIAL_MAP_DIFFUSE` e `MATERIAL_MAP_NORMAL`, che
è già quello che il fragment shader legge da `texture0` e `texture2` dalla
fase 1.

## Cosa non cambia

- Il mondo cotto. Nessun dato nuovo su disco.
- `BUILD_CELL` e la geometria dei pezzi: le case restano 3×2 e 4×3 celle da
  2,6 m, e la collisione con loro non si tocca.
- Il passaggio d'ombra: scrive profondità, non colore.
- I prop con UV vere.

## Le prove

Nello stile di `alfa.c` e `normalmap.c`, che aprono una finestra, disegnano e
contano pixel — e che escono 77 dove non c'è un contesto OpenGL:

1. **L'asse scelto è quello atteso.** Un quadrato con normale +Y prende la
   proiezione XZ; uno con normale +X prende ZY. Si prova con una texture
   asimmetrica, perché una a scacchi non distingue una proiezione dall'altra.
2. **Ruotare l'istanza non ruota il motivo.** È la proprietà che giustifica lo
   spazio oggetto: due istanze della stessa mesh, una a 0° e una a 40°, devono
   dare lo stesso motivo sulla propria faccia. Senza questa prova, il giorno
   che qualcuno "semplifica" proiettando in mondo non se ne accorge nessuno.
3. **Due istanze in posizioni diverse ricevono sfalsamenti diversi.** Altrimenti
   il punto 4 del design è scritto e non fatto.
4. **L'interruttore spento lascia il rendering identico a prima.** È la garanzia
   che massi, cespugli ed erba non cambino di un pixel.

Da verificare sabotando, come le altre.

## Fuori ambito

- **La torre col forte**: `modular_fort_01`, indirizzando i pezzi per indice
  invece che per sorteggio. È il lavoro successivo, e questo design gli prepara
  la strada senza dipenderci.
- **I graticci a vista**: vogliono UV o decal, e la proiezione non li sa fare.
- **La cripta**: userà lo stesso interruttore, ma è il sottoprogetto 4.
- **Case in pietra o intonaco**: è una tabella di materiali diversa, non un
  design diverso.

## Documenti collegati

- `docs/01-architettura.md` — sezioni *Normal map*, *Instancing*, *Varianti*
- `docs/03-asset-pubblici.md` — il catalogo misurato e come si sceglie un asset
- `docs/superpowers/specs/2026-09-05-varianti-prop-design.md` — il raggruppamento
  delle mesh, che qui serve a leggere i kit modulari
