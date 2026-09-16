# Le derivate su una posizione relativa alla camera — design

**Data:** 2026-09-16
**Fase:** riparazione della regressione lasciata dalla domanda F
**Domanda che tocca:** la F di `docs/06-stato-e-prossimi-passi.md`, che è
chiusa come lavoro ma lascia dietro di sé due cose. Questa è la prima: la
**regressione fp32 in campo vicino e lontano dall'origine**. L'altra — le
tangenti morte — resta dov'è, e il perché sta in *Cosa resta fuori*.

## Perché

Il 9 settembre `SurfaceNormal()` ha smesso di leggere `fragTangent` e ha
iniziato a costruire la terna tangente dalle **derivate di schermo**: `dFdx` e
`dFdy` della posizione nel mondo e delle UV. Il motivo era
`UpdateModelAnimation()`, che aggiorna posizioni e normali ma non le tangenti,
e il cambiamento ha tolto quel difetto per costruzione.

Ha però introdotto una dipendenza che la tangente interpolata non aveva: la
terna ora dipende dalla **precisione della posizione nel mondo**, e quella
posizione è assoluta in metri.

### Il conto

`fragPosition` esce dal vertex shader come coordinata di mondo e arriva a
`WORLD_SIZE` = 4096 (`src/config.h:33`). In fp32 un ulp vale `x · 2⁻²³`: a
`x ≈ 3000` sono circa **3,6·10⁻⁴ m**.

Il passo di mondo per pixel, con `fovy = 70°`, vale
`2 · tan(35°) · d / righe`. Sui 720 di `config.h:16` fa `d · 1,9·10⁻³ m`, su
1080 righe `d · 1,3·10⁻³ m`. **A un metro dalla superficie il passo vale tre o
quattro ulp**; a mezzo metro uno e mezzo o due.

E `dFdx(fragPosition)` è la differenza di due varying **già arrotondati**, con
un errore dell'ordine dell'ulp su un passo che di ulp ne conta tre: la terna
può arrivare con un errore relativo del **15–30%**.

### Dove si vede, e perché è proprio il caso che conta

È il caso per cui il lavoro del 9 settembre esiste: **un volto a uno o due
metri.** Ne risentono anche i prop Poly Haven a cui ci si accosta,
`crypt.gltf` — dentro la cripta le pareti stanno sotto il metro — e
`BUILD_KEEP` / `BUILD_STATUE`, che in `src/world.c:295-296` hanno `mode 0` e
`uvVere = true` e quindi passano da `SurfaceNormal()` con normal map vera.

Il terreno no: riceve la normale piatta di riserva, e lì la terna conta a meno
di quattro millesimi.

## L'errore da non rifare: sottrarre nel fragment

**`fragPosition - viewPos` dentro il fragment shader non ripara niente.** Il
varying è già stato interpolato e arrotondato a fp32 con magnitudine 3000: i
bit sono persi *prima* che il fragment veda il numero, e sottrarre la camera dà
un numero piccolo e sbagliato uguale. La sottrazione deve avvenire **prima**
del varying, cioè nel vertex shader.

Va scritto perché è la strada corta che viene in mente per prima, costa una
riga, e sembra funzionare finché non si misura.

## L'errore da non rifare, secondo: calcolare `world` e poi sottrarre

Neanche `fragPosRel = world - viewPos` è la risposta piena, ed è la parte non
ovvia di questo lavoro.

`world` è già arrotondato a ulp(3000) ≈ 3,6·10⁻⁴ m nel momento in cui viene
formato. La sottrazione successiva fra due numeri vicini è esatta — Sterbenz —
ma esatta **su un operando già sporco**: l'errore sopravvive intatto nel numero
piccolo.

La cura è **non formare mai il numero grande**. La posizione relativa si
costruisce sommando due quantità piccole:

```glsl
/* scene.vs — matModel è affine: mat3() ne prende la parte lineare,
 * [3].xyz la traslazione. */
vec3 local = mat3(matModel) * vertexPosition;        /* metri, piccolo */
fragPosRel = local + (matModel[3].xyz - viewPos);    /* piccolo + piccolo */

/* scene_inst.vs — `local` esce dalla riga che già c'è per `world` */
vec3 local = RuotaY(vertexPosition * sc, s, c);
vec3 world = local + instPosSin.xyz;                 /* serve a gl_Position */
fragPosRel = local + (instPosSin.xyz - viewPos);
```

Le due posizioni grandi si annullano in una sottrazione fra numeri vicini; il
resto vive su magnitudini di metri, dove un ulp vale 10⁻⁷.

### Perché questo sposta l'errore dove non fa danno

L'errore residuo diventa **per vertice**, non per frammento. Un varying si
interpola linearmente fra i tre vertici, quindi un errore per vertice produce
sulla superficie del triangolo un campo d'errore **lineare**, e la derivata di
una funzione lineare è una costante: un bias piccolo, pari allo scarto fra i
vertici diviso per l'estensione in pixel del triangolo.

Oggi invece l'errore è **indipendente pixel per pixel**, perché ogni frammento
arrotonda il proprio valore interpolato: è rumore, e il rumore sopravvive alla
differenza fra pixel adiacenti che è esattamente ciò che `dFdx` calcola.

**Questa è la ragione per cui la correzione funziona**, ed è più importante del
fattore 10⁻⁷ contro 10⁻⁴: un errore grande ma liscio sulla superficie di un
triangolo non disturba una derivata; un errore piccolo ma scorrelato sì.

## Che cosa cambia, file per file

| file | modifica |
|---|---|
| `assets/shaders/scene.vs` | `uniform vec3 viewPos;`; `out vec3 fragPosition` diventa `out vec3 fragPosRel`, costruita come sopra. `gl_Position` intoccato |
| `assets/shaders/scene_inst.vs` | idem, con `local` estratto dalla riga di `world`, che resta per `gl_Position` |
| `assets/shaders/scene.fs` | `in vec3 fragPosRel;`; derivate su `fragPosRel`; `dist = length(fragPosRel)`; una riga `vec3 wp = fragPosRel + viewPos;` issata in cima a `ShadowFactor()` per le tre ricerche d'ombra |

**Nessuna riga di C.** `viewPos` è già un uniform impostato una volta per
fotogramma su **entrambi** i programmi (`src/light.c:464`), e un uniform è del
programma linkato, non dello stadio: dichiararlo anche nel vertex shader lo
trova sulla stessa location, che `light.c:108` ha già preso.

### Perché rinominare invece di cambiare il contenuto sotto lo stesso nome

`fragPosition` significa "posizione nel mondo" in tre file e cinque punti d'uso.
Cambiarne il significato lasciandole il nome è il modo di fare in cui un uso
rimasto indietro continua a compilare e sbaglia in silenzio. Col nome nuovo
**non compila**, ed è la rete di questo lavoro: non c'è nessuna prova che possa
prendere un uso dimenticato di una posizione assoluta là dove ne serviva una
relativa, perché il difetto sarebbe una normale leggermente storta.

### Le tre ricerche d'ombra

`ShadowFactor()` moltiplica `lightVP0` e `lightVP1` per la posizione assoluta,
e quella serve assoluta: le matrici della luce vivono in coordinate di mondo.
Si ricostruisce con `fragPosRel + viewPos` in una riga sola issata in cima alla
funzione — non tre volte — e la sua precisione è quella di oggi, né meglio né
peggio. La distanza invece migliora e costa meno: `length(fragPosRel)`
sostituisce `length(fragPosition - viewPos)`.

### Cosa non è toccato

- **Il passaggio d'ombra**: `main()` esce prima di `ShadowFactor()` quando
  `depthOnly == 1`, e `SurfaceNormal()` non viene mai chiamata lì;
- **I materiali proiettati** (`projMode` 1 e 2): non passano da
  `SurfaceNormal()`, costruiscono la terna dagli assi della proiezione;
- **Il terreno**: riceve la normale piatta;
- **Il ripiego sulle UV degeneri**: condizione e ripiego invariati. Resta
  scoperto esattamente come oggi.

## Il costo: previsione, non misura

Dichiarata qui, prima di scrivere il codice, e **non verrà misurata** — la
decisione è presa e va scritta col suo perché, o qualcuno la scambierà per una
dimenticanza.

| stadio | variazione |
|---|---|
| fragment | −1 sottrazione vec3 (`dist`), +1 addizione vec3 (issata) → **pareggio** |
| vertex | +1 sottrazione vec3 e +1 addizione vec3 per vertice |

**Previsione: nessuna variazione misurabile sul passaggio principale.** La
ragione per cui il lato vertice non preoccupa sta in `docs/06`: i personaggi
sono il costo dominante del fotogramma, e lo sono per frammento — togliendoli
dal blocco d'ombra il passaggio scende da 3,3 a 0,26 ms. Due operazioni
vettoriali per vertice non si vedono su quel bilancio.

Se un giorno qualcuno misurasse e trovasse il contrario, questa previsione è il
posto dove dirlo.

## Come si verifica: una terza modalità del banco

Le due modalità esistenti non rispondono alla domanda. *Costo* misura
millisecondi; *rumore* misura la varianza fra fotogrammi **consecutivi**. Qui
serve confrontare due fotogrammi che **in aritmetica esatta sarebbero
identici**, e differiscono solo per dove stanno nel mondo.

### L'esperimento

Un terzo diff di strumentazione in `tools/banco/`, acceso da
`FROSTMARK_BANCO=fp32`:

- disegna **un solo prop** — un `namaqualand_boulder_04`, normal map vera,
  `projMode == 0` — alla posizione di mondo letta da `FROSTMARK_FP32_POS`;
- **niente terreno, niente entità, niente ombre** (`shadowOn = 0`);
- camera piazzata **relativa al masso**: stessa distanza, stesso sguardo,
  stesso sole nelle due esecuzioni;
- `LoadImageFromScreen()` salvato in PNG.

Due esecuzioni: `64,64` e `4032,4032`. Tutto è relativo al masso, quindi in
aritmetica esatta i due PNG sono identici bit per bit. Quello che li separa è
solo fp32.

### Perché terreno, entità e ombre vanno spenti

Non è pulizia: sono differenze **vere** fra i due posti, e maschererebbero
quella cercata. Il terreno ha quota e normale diverse a `64,64` e a
`4032,4032`; le matrici della luce dipendono dalla posizione assoluta, quindi
la stessa geometria riceve ombra diversa. Lasciandoli accesi il banco
misurerebbe la differenza fra due panorami, che non è la domanda.

### La condizione d'arresto, dichiarata adesso

Si gira **prima sul binario di oggi**, a un metro e a mezzo metro. Se i due PNG
risultano identici, **il difetto è teorico**: si scrive il numero in `docs/06`,
si dichiara che il conto degli ulp prevedeva un errore che in pratica non si
manifesta, e **il lavoro si ferma lì senza toccare gli shader**.

Dichiararla prima è il punto. Una soglia scritta dopo aver visto il numero non
predice niente, e questo progetto ha già un precedente in cui una conclusione
era giusta per la ragione sbagliata — il vincolo 2 di `docs/06`, gli alberi.

### I tre numeri, non uno

| confronto | cosa dice | atteso |
|---|---|---|
| oggi vicino **vs** oggi lontano | l'ampiezza del difetto | differiscono, o il lavoro si ferma |
| dopo vicino **vs** dopo lontano | che la correzione funziona | identici |
| oggi vicino **vs** dopo vicino | che la correzione non ha cambiato ciò che non doveva | identici o quasi |

Il terzo è quello che prende gli errori di segno. Vicino all'origine la
posizione relativa e quella assoluta quasi coincidono: se lì il risultato
cambia, non è la precisione, è un `+` diventato `−` o una `local` sbagliata.

Si riportano differenza media in livelli e frazione di pixel cambiati, come in
modalità rumore.

### La trappola di questa modalità

**Un masso che riempie poco lo schermo misura il cielo.** Il fenomeno vive sui
pixel dove la normal map è campionata con una terna costruita dalle derivate:
se il masso occupa cento pixel su un milione, la differenza media sull'immagine
intera è divisa per diecimila e sembra zero. L'inquadratura va scelta perché il
masso **riempia il fotogramma**, ed è il motivo per cui la distanza è un metro:
non è solo il caso che conta, è anche l'unico in cui la misura ha segnale.

## Cosa resta fuori

- **Le tangenti morte** — `BuildTangents()`, l'attributo `vertexTangent`, il
  varying `fragTangent`. `docs/06` lega la loro rimozione alla domanda C — il
  percorso nuovo provato in gioco su un personaggio con una normal map vera — e
  toglierle tocca la disposizione degli attributi in `src/instancing.c`, che è
  delicata. **Questo lavoro non le sfiora**, e va detto perché la tentazione è
  forte: si sta già lavorando nei tre stessi file. Farlo qui renderebbe
  inattribuibile qualunque differenza il banco mostrasse.
- **Le UV degeneri.** Restano scoperte, con la stessa condizione e lo stesso
  ripiego.
- **Il banco del costo.** Sostituito dalla previsione a somma nulla qui sopra.
- **Una prova in `tools/prove/`.** Nessuna: sarebbe la prima a rileggere il
  framebuffer, e il banco risponde già. Se la regressione tornasse, è il banco
  che la riprende — ed è un banco, cioè va lanciato a mano, il che è un limite
  dichiarato e non un difetto nascosto.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md`, domanda **F** — dove la regressione è
  descritta e da dove viene il conto degli ulp;
- `docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md` — il lavoro
  che ha introdotto le derivate, la misura del costo con la soglia dichiarata
  prima, e la proposta di questa verifica;
- `tools/banco/README.md` — il banco, le due modalità esistenti e il metodo;
- `docs/01-architettura.md`, sezione *Normal map* — come funziona oggi.
