# Il banco di misura delle prestazioni

Questa cartella non contiene un programma. Contiene i **diff** che trasformano
i sorgenti del gioco in un binario strumentato, e le istruzioni per rifarlo.
Chi ha bisogno di un numero di prestazioni — "questo cambiamento costa
quanto?", "questo shader fa tremolare l'immagine?" — parte da qui invece di
reinventare lo strumento.

## A che serve, e perché non è un bersaglio del Makefile

`make` costruisce il gioco. Strumentare vuol dire **modificare i sorgenti**:
aggiungere un cronometro attorno al passaggio di disegno, forzare lo stato di
gioco, far ruotare la visuale da sola, leggere lo schermo fotogramma per
fotogramma. Nessuna di queste modifiche deve mai finire nel gioco che gioca
qualcuno — non dietro un flag, non dietro una `#define` spenta di default: un
`glFinish()` lasciato acceso per sbaglio costerebbe prestazioni vere a chi
gioca, per misurare le prestazioni di chi sviluppa.

Per questo il banco non è un bersaglio del Makefile né un file che vive dentro
`src/`. È una **procedura**: si applicano due diff a due copie dei sorgenti
tenute *fuori* dal repository, si compila lì, ci si misura lì, e le copie si
buttano. Quello che resta nel repo sono i diff — il metodo, non l'eseguibile.

## Come si costruisce

Due alberi di sorgenti separati, uno per ciascuna versione che si vuole
confrontare (per esempio due commit, o lo stesso commit con uno shader
diverso):

```bash
mkdir -p /tmp/banco/prima /tmp/banco/dopo
git archive <commit-prima> | tar -x -C /tmp/banco/prima
git archive <commit-dopo>  | tar -x -C /tmp/banco/dopo
```

`git archive` porta solo quello che è tracciato. Due cose non lo sono e vanno
sistemate a mano in **entrambe** le copie, o il binario non parte o carica gli
asset sbagliati:

- **`assets/models/` e `assets/textures/`** sono in `.gitignore` (insieme a
  `assets/fonts/`, `assets/audio/`, `assets/heightmap.png`): vanno copiati a
  mano dalla checkout di lavoro dentro ciascuna copia, identici nelle due;
- **`vendor/raylib`** è il submodule dei sorgenti di raylib, anch'esso escluso
  da `git archive`. Non ha senso ricompilarlo due volte: si collega con un
  link simbolico dalla checkout di lavoro (o da una copia compilata una sola
  volta) dentro ciascun albero —
  `ln -s /percorso/alla/checkout/vendor/raylib /tmp/banco/prima/vendor/raylib`
  e lo stesso per `dopo`.

A questo punto in ciascuna copia si applica il diff di questa cartella e si
compila con `make` normale:

```bash
cd /tmp/banco/prima && patch -p1 < strumentazione-main.c.diff \
                     && patch -p1 < strumentazione-game.c.diff && make
cd /tmp/banco/dopo  && patch -p1 < strumentazione-main.c.diff \
                     && patch -p1 < strumentazione-game.c.diff && make
```

(I diff qui dentro sono stati generati con `diff -u` fra un originale e una
copia modificata, non con `git diff`; `patch -p1` li applica lo stesso. Se il
sorgente della copia si è mosso da quello con cui i diff sono stati fatti —
è successo una volta, il 9 settembre 2026 — si applicano a mano leggendo le
`@@` come guida, che è il motivo per cui il resto di questo file spiega le
modifiche una per una invece di limitarsi a dire "applica il diff".)

`assets/world/` (il mondo cotto) è tracciato e arriva con `git archive`, non
va copiato a mano.

## Le modifiche di strumentazione

Entrambi i diff sono commentati nel punto in cui toccano il codice — i
commenti dicono `BANCO` — ma vale la pena spiegare perché ciascuna c'è.

**`strumentazione-game.c.diff`**

- **Il cronometro.** Attorno al **solo** blocco `BeginMode3D(g->cam) …
  EndMode3D()` dentro `DrawScene()` viene messo un `glFinish()` prima e uno
  dopo, con `GetTime()` in mezzo. `glFinish()` serializza la GPU: senza,
  l'orologio a parete misurerebbe quanto ci mette la CPU ad *accodare* i
  comandi di disegno, non quanto ci mette la GPU a eseguirli, perché OpenGL è
  asincrono. Il passaggio d'ombra resta **fuori** dal cronometro apposta: in
  `scene.fs`, `SurfaceNormal()` — la funzione che questo lavoro tocca — non
  viene mai chiamata con `depthOnly == 1`, quindi cronometrare anche l'ombra
  diluirebbe la differenza che si vuole vedere con del tempo che non può
  cambiare.
- **`GS_PLAY` forzato** in due punti — `GameInit()` e `GameNewWorld()` —
  perché altrimenti il binario si fermerebbe al menu principale e non
  disegnerebbe mai la scena di gioco.

**`strumentazione-main.c.diff`**

- **`FLAG_VSYNC_HINT` tolto e `SetTargetFPS(60)` tolto**, sostituiti da
  `FLAG_WINDOW_HIDDEN`. Col vsync acceso la GPU sta ferma metà del tempo ad
  aspettare lo schermo, e si raccolgono meno campioni nello stesso numero di
  secondi; la finestra nascosta toglie di mezzo il compositore, che altrimenti
  aggiungerebbe un costo di composizione estraneo al gioco.
- **La rotazione dello yaw.** In modalità costo il giocatore resta fermo al
  punto di nascita e `g->player.yaw` avanza di un passo fisso (0,013 rad) a
  ogni fotogramma: così l'insieme delle direzioni viste è lo stesso in tutte
  le esecuzioni, indipendentemente da quanto è veloce la macchina che gira il
  banco.
- **Le due modalità**, lette dalla variabile d'ambiente `FROSTMARK_BANCO` — la
  sezione seguente le spiega.

## Le due modalità

Una sola variabile d'ambiente sceglie cosa fare, letta in `main()`:

### `FROSTMARK_BANCO` assente — modalità **costo**

Il giocatore è fermo, la visuale ruota, si gira per **75 secondi** e alla fine
si stampa una riga:

```
fotogrammi=<N> passo_medio_ms=<tempo medio del passaggio principale>
```

`passo_medio_ms` è la somma dei `glFinish()`-a-`glFinish()` divisa per il
numero di fotogrammi: il numero da confrontare fra le due copie.

```bash
./frostmark            # modalita' costo, 75 s
```

### `FROSTMARK_BANCO=rumore` — modalità **rumore**

Posizione fissa e riproducibile — non scelta a caso: nel campo visivo ci sono
51 prop Poly Haven con normal map vera (37 massi e 14 cespugli, da 17 a 136 m)
sul nevaio a nord-est, senza alberi di cartone a coprirli. Quaranta
fotogrammi di riscaldamento, poi **60 fotogrammi catturati** con
`LoadImageFromScreen()`, con la camera che avanza di 1 cm a fotogramma lungo
lo sguardo. Per ogni coppia di fotogrammi consecutivi si calcola la
differenza media assoluta sulla luminanza (`0,2126R + 0,7152G + 0,0722B`) su
tutti i pixel, e si stampa:

```
diff_media=<...> coppie=<...> pixel=<...> frazione_cambiati=<...> diff_max=<...>
```

`diff_media` è il numero da confrontare fra le due copie. Due variabili
d'ambiente opzionali:

- `FROSTMARK_SCATTO=percorso.png` — salva il primo fotogramma catturato, per
  controllare a occhio che l'inquadratura sia quella giusta;
- `FROSTMARK_MAPPA=percorso.bin` — salva una mappa per pixel (float32 raw,
  largo × alto, ordine riga per riga) della differenza media accumulata: dice
  *dove* sta il rumore misurato, non solo quanto vale in media.

```bash
FROSTMARK_BANCO=rumore FROSTMARK_SCATTO=prima.png FROSTMARK_MAPPA=prima.bin ./frostmark
```

## Il metodo

Questa parte vale più del codice: i diff si possono riscrivere in un'ora, il
modo di leggerne l'output no.

**La misura è relativa, non assoluta.** Il banco confronta due alberi di
sorgenti con lo stesso strumento addosso — non produce un numero che si possa
mettere accanto a una misura fatta mesi fa, con un'altra macchina, un altro
driver, un'altra scena. Ogni volta che serve un confronto si costruiscono due
copie e si misurano insieme, nella stessa sessione, sulla stessa macchina.

**`glFinish()` distorce il tempo assoluto, e non importa.** Serializzare la
GPU rallenta ogni fotogramma rispetto a un gioco vero, dove la CPU accoda
lavoro futuro mentre la GPU finisce quello vecchio. Ma la distorsione è la
stessa nelle due copie: la domanda a cui il banco risponde è *quanto cambia*
il passaggio, non *quanto vale* in assoluto.

**L'ordine dei tre passi conta, ed è questo:**

1. **Si misura il rumore di fondo** — più giri della stessa copia, non
   toccata, uno dietro l'altro.
2. **Si dichiara la soglia**, e la si scrive e la si committa da sola, prima
   di guardare il risultato della copia modificata. Una soglia scritta *dopo*
   aver visto il numero non predice niente: descrive quello che il codice ha
   già fatto, che è un'altra cosa.
3. **Solo allora si misura** il cambiamento vero, copia contro copia.

**La macchina deriva mentre si scalda, e la deriva è più grande del rumore fra
giri consecutivi.** Nel lavoro del 9 settembre 2026, tre giri consecutivi
della stessa copia non toccata davano un'escursione dell'1,7% — sembrava il
rumore di fondo. Ma erano i primi tre punti di una rampa monotona: fra il
primo e l'ultimo di **sei** giri della stessa copia la deriva era del **4,1%**,
più del doppio di quel 1,7%, e vicina alla soglia dichiarata (+5%). Con un
risultato vicino a quella soglia, tre giri consecutivi non avrebbero potuto
decidere niente. Il rimedio: misurare le due copie a **coppie alternate** —
prima, dopo, prima, dopo — invece che tutti i giri di una copia e poi tutti
quelli dell'altra. E quando la deriva esiste, va guardato **da che parte
spinge**: se spinge contro il segno del risultato osservato (per esempio la
macchina scalda e rallenta, ma la copia misurata per ultima — quindi più
calda — risulta comunque più veloce), il risultato è un **minorante**: la
differenza vera è almeno grande quanto quella misurata, verosimilmente di
più.

**Niente `GameInput()` e niente `GameSimulate()` in modalità costo.** Se
l'input o la fisica girassero, al secondo 75 le due copie — che partono dallo
stesso stato ma girano a velocità leggermente diverse — sarebbero arrivate a
stati di gioco diversi, e starebbero disegnando scene diverse. Il confronto
varrebbe zero. Per questo in modalità costo il giocatore è fermo e solo lo yaw
ruota, per un passo fisso indipendente dal framerate.

**Le due copie devono differire SOLO in ciò che si sta misurando.** Tutto il
resto — strumentazione, dati, asset, flag di compilazione — deve essere
identico. Nel lavoro del 9 settembre 2026 i due eseguibili compilati sono
risultati **byte per byte identici** (`cmp` pulito): l'unica differenza fra le
due esecuzioni era `assets/shaders/scene.fs`, un file di testo letto a
runtime, non compilato dentro il binario. Se i due eseguibili differissero in
qualcos'altro — un flag diverso, una versione diversa di raylib, un mondo
cotto diverso — la misura non direbbe più nulla sulla cosa che si voleva
isolare.

### Due trappole della modalità rumore

- **Un'inquadratura senza gli oggetti giusti misura terreno e cielo.** Il
  fenomeno che questa modalità cerca — lo sfarfallio dei triangoli sotto il
  pixel — esiste solo dove la normal map è vera. Puntare la camera altrove dà
  un numero, ma è il numero di un'altra cosa.
- **Un passo di camera troppo grande annega il fenomeno nel movimento.** Il
  movimento (1 cm/fotogramma qui) serve — ferma, la camera darebbe fotogrammi
  identici e varianza zero in entrambe le copie — ma se il passo è troppo
  grande il tremolio da movimento normale diventa enormemente più grande del
  fenomeno che si vuole isolare, e la differenza fra le due copie sparisce
  dentro il rumore del movimento stesso.

## Il precedente

Questo banco è nato e ha girato il 9 settembre 2026 per il lavoro che ha
sostituito le tangenti interpolate con la terna costruita dalle derivate di
schermo (`docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md`).
Ha risposto a due domande: quanto costa il cambiamento sul passaggio
principale — **da 2,539 a 2,465 ms, −2,9%**, soglia dichiarata +5% rispettata
con il segno opposto a quello temuto — e se il cambiamento fa tremolare
l'immagine sui bordi dei massi — **la differenza media fra fotogrammi
consecutivi passa da 0,23933 a 0,24000 livelli**, +0,25%, sessanta volte sotto
il tremolio normale da movimento. Per il resto — il rumore di fondo, le sei
misure alternate, la mappa per pixel della varianza — vedi la spec.

La stessa spec propone una verifica non ancora eseguita, che questo banco
rende possibile: misurare lo stesso masso a un metro di distanza, una volta
vicino all'origine del mondo e una volta all'angolo lontano della mappa, per
vedere se la terna costruita dalle derivate degrada con il modulo della
posizione (la regressione fp32 discussa in `docs/06`, sezione **F**).
