# Varianti dei prop — design

**Data:** 2026-09-05
**Fase:** 3 del piano "asset realistici" (fasi 1 e 2 fatte)
**Domanda che chiude:** la A di `docs/06-stato-e-prossimi-passi.md`

## Perché

Metà del catalogo vegetale di Poly Haven non è un oggetto: è un **set di
varianti**. `shrub_02` sono quattro cespugli diversi in fila su sei metri,
`periwinkle_plant` sei piante affiancate su 1,2. Il gioco li carica come un
oggetto unico, quindi dove dovrebbe esserci un cespuglio ne compaiono quattro
in miniatura, allineati. Provato e rimosso: oggi il sottobosco resta quello del
kit.

Ma il set di varianti non è un ostacolo da aggirare — è **esattamente quello
che serve a un bosco**. Quattro cespugli diversi valgono più dello stesso
cespuglio ripetuto cinquemila volte, e il motore ha già quasi tutto: dalla fase
3 ogni coppia (mesh, materiale) finisce nel suo lotto. Manca una cosa sola:
scegliere quale variante disegnare per ogni istanza.

Sbloccare i set vuol dire sbloccare metà del catalogo vegetale, che è la metà
che il giocatore guarda da vicino mentre cammina.

## Obiettivo

Un prop il cui asset contiene N varianti ne mostra **una sola** per istanza,
scelta dalla posizione, alla taglia dichiarata. Un prop il cui asset ne contiene
una si comporta esattamente come oggi.

Vale per **qualunque** tipo di prop, non per una lista scritta a mano: il
motore riconosce da solo se un asset è un set.

## Cos'è una variante

`Mesh` di raylib 5.5 non porta il nome (`raylib.h:345`): dopo `LoadModel()`
l'unica informazione rimasta è la geometria. La variante si riconosce quindi
dall'ingombro:

> **Due mesh appartengono alla stessa variante se i loro ingombri XZ si
> toccano.**

La relazione è **transitiva**: tre mesh in fila in cui la prima tocca la
seconda e la seconda la terza fanno un gruppo solo. Il contatto si valuta senza
tolleranza aggiunta — due ingombri che si sfiorano appartengono allo stesso
gruppo. Il rischio è dichiarato: due varianti modellate a contatto finirebbero
insieme, e si vedrebbe subito, perché comparirebbero sempre in coppia.

Su `shrub_02` dà quattro gruppi, uno per cespuglio. Su `nettle_plant`, che ha
sei mesh sovrapposte perché sono i materiali di una pianta sola, dà un gruppo.
La regola distingue *sei piante affiancate* da *una pianta in sei pezzi* senza
sapere niente delle due.

I gruppi si ordinano per X crescente. Non è estetica: l'ordine decide quale
indice tocca a quale variante, e deve essere lo stesso a ogni esecuzione e su
ogni piattaforma, altrimenti lo stesso mondo salvato mostrerebbe cespugli
diversi.

**Un gruppo solo significa il comportamento di oggi**, il che rende il caso
degenere anche il ripiego: se un asset non si separa, si disegna intero come
prima.

### Perché non le alternative

- *Una mesh = una variante* spezzerebbe `nettle_plant` in sei pezzi di pianta
  e `grass_medium_01` in diciassette.
- *Dichiarato a mano in `gExtProp`* costringerebbe a misurare e trascrivere
  ogni asset nuovo, contro la direzione presa quando le scale hanno smesso di
  tararsi a mano.

## La taglia

Ogni variante si scala dal **proprio** ingombro fino alla dimensione dichiarata
in `gExtProp`, non dall'ingombro del set. Un cespuglio alto 1,4 m resta alto
1,4 m qualunque variante esca.

È la scelta che tiene insieme disegno e collisione. `p->radius` è cotto nel file
del mondo (`worldtypes.h:38`) e la collisione lo legge a `world.c:1341`: se le
varianti avessero taglie diverse, il masso disegnato e il masso solido
divergerebbero. Così cambia la forma, non la misura, e il mondo cotto resta
valido senza rigenerarlo.

## Ricentrare

raylib, caricando un glTF, **fonde le trasformazioni dei nodi dentro i
vertici** (`rmodels.c`, `cgltf_node_transform_world`). È la proprietà su cui
poggia `InstModel`: tutte le mesh di un modello condividono un'origine, quindi
una sola trasformazione d'istanza vale per tutte. Ed è anche il motivo per cui
i set non funzionano: la seconda variante di `shrub_02` porta cucito nei
vertici l'offset di due metri che la mette in fila.

Al caricamento, per ogni gruppo si sottrae dai vertici il centro XZ del gruppo
e il suo minimo Y, e si rifà l'upload con `UpdateMeshBuffer()`. Una volta sola,
prima di creare i lotti.

L'alternativa era un uniform di offset per lotto, letto dal vertex shader. È
stata scartata: costa un tocco allo shader e una sottrazione per vertice per
fotogramma, per risparmiare una passata sui vertici fatta una volta al
caricamento.

Ricentrare tocca solo le posizioni: normali e tangenti non cambiano, quindi
`LightApplyToModel()` e `BuildTangents()` restano dove sono e come sono.

### La conseguenza sul ripiego

Il percorso non instanziato di `world.c:768-779` disegna oggi il modello intero
con `DrawModelEx()`. Ricentrate, le varianti si accavallerebbero tutte
nell'origine. Diventa un giro di `DrawMesh()` sulle sole mesh del gruppo
scelto, con la matrice costruita a mano.

Non è un percorso morto: è quello che gira quando `assets/shaders/` manca.

## La scelta

```c
static int PropVariantOf(const Prop *p, int n);   /* FmHash01(pos, sale 91) * n */
```

Funzione **pura della posizione**, con lo stesso trucco di `HouseShapeOf()`
(`world.c:664`). Disegno, passaggio d'ombra e collisione la richiamano e non
possono divergere; il mondo cotto non cambia di un byte e nessun dato nuovo
entra nel salvataggio.

Il sale 91 la rende indipendente dalle altre decisioni prese dalla posizione:
la variante non deve correlare con la forma delle case.

L'imbardata è già casuale per prop, quindi le due varietà si moltiplicano:
quattro varianti girate a caso non si leggono come quattro.

## Struttura del codice

`world.c` è a 1.371 righe. La parte geometrica va in un modulo nuovo:

**`src/meshgroup.c` / `.h`** — puro, senza OpenGL e senza raylib oltre ai tipi:
dato un array di `BoundingBox`, torna i gruppi ordinati per X, con il loro
ingombro complessivo. È la parte che si può provare senza un contesto grafico,
e le prove che non lo trovano escono 77.

**`world.c`** tiene la politica: quale variante tocca a un prop, come si
disegna, come si carica.

**`instancing.h`** guadagna una sola funzione:

```c
bool InstModelCreateSubset(InstModel *im, Model m, const int *meshIdx, int n);
```

`InstModelCreate()` diventa il caso "tutte le mesh". Il resto del modulo non si
tocca.

**`world.h`**: `InstModel propBatch[PROP_COUNT]` diventa un descrittore per
tipo — array dinamico di `InstModel`, una scala per variante, e gli indici
delle mesh di ogni gruppo, che servono al ripiego. Dinamico e non a tetto fisso
perché `grass_medium_01` ha diciassette mesh e un tetto scelto a occhio si
sbaglia una volta sola ma in silenzio.

## Le prove

In `tools/prove/`, senza contesto OpenGL:

1. quattro ingombri in fila, separati → quattro gruppi, nell'ordine delle X;
2. ingombri sovrapposti → un gruppo solo;
3. il ricentraggio porta il centro XZ del gruppo a zero e il minimo Y a zero;
4. la scala per variante porta ogni gruppo alla taglia dichiarata, partendo da
   ingombri diversi fra loro;
5. la scelta è deterministica per posizione, e su molte posizioni copre tutte
   le varianti invece di concentrarsi su una.

**Da verificare sabotando il codice**, come le altre: una prova che non prende
niente è peggio di nessuna prova. Attenzione al caso analogo a quello del cubo
nella prova dell'instancing — un ingombro cubico non si accorge se si scambia
l'altezza con la larghezza. Gli ingombri delle prove vanno fatti asimmetrici.

## Fuori ambito

- **Pesi per variante** (una comune, tre rare): YAGNI finché non si vede il
  bisogno guardando il bosco.
- **Varianti dichiarate a mano**: rientrerebbe solo se la regola geometrica
  sbagliasse su un asset vero.
- **Spezzare le mesh oltre i 65.535 vertici**: è la domanda B di
  `docs/06-stato-e-prossimi-passi.md`, e resta aperta. Nessun albero del
  catalogo entra nel gioco per questa strada.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md` — le tre domande aperte, questa è la A
- `docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md` — il
  perché dei lotti, e le misure sul catalogo
- `docs/01-architettura.md` — sezioni *Instancing* e *Le prove*
