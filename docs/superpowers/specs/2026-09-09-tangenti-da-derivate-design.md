# Le tangenti dalle derivate di schermo — design

**Data:** 2026-09-09
**Fase:** prerequisito dei personaggi realistici
**Domanda che chiude:** la F di `docs/06-stato-e-prossimi-passi.md`

## Perché

`UpdateModelAnimation()` di raylib aggiorna le posizioni e le normali di una
mesh riggata, ma **non le tangenti**. Un personaggio con una normal map vera
avrebbe quindi il rilievo fermo alla posa di riposo: la superficie si muove, il
suo microrilievo no, e si vedrebbe sui volti — dove la normal map è metà di
quello che rende realistico un asset.

**Questo lavoro viene prima di scaricare qualunque personaggio.** La domanda C
— i personaggi riggati, che è la più grossa rimasta — sceglie una fonte
(Mixamo, MakeHuman, scansione più auto-rig) e ne porta in gioco dei modelli con
materiali PBR completi. Portarli prima significherebbe scoprire il difetto
dopo, con i modelli già dentro, e doverlo riparare mentre si guarda un volto
storto.

### Perché oggi non si vede niente

**Nessun personaggio in gioco ha una normal map vera.** `LightApplyToModel()`
costruisce le tangenti solo per le mesh che ne hanno una
(`src/light.c:350-356`), e i sei modelli KayKit non ne hanno: ricevono la
normale piatta di riserva installata da `FitFlatNormal()`, quindi `ts` vale
`(0,0,1)` e `SurfaceNormal()` restituisce esattamente la normale del vertice.

Ne discende una cosa che va detta forte, o chi riprende perde tempo:
**questo è un prerequisito, non una riparazione.** Non esiste in gioco un
difetto da andare a cercare. Il difetto esiste nel *prossimo* asset, non in
questo.

## Obiettivo

Nel ramo `projMode == 0` di `assets/shaders/scene.fs`, `SurfaceNormal()` smette
di leggere `fragTangent` e costruisce la terna dalle **derivate di schermo** —
`dFdx` e `dFdy` della posizione nel mondo e delle coordinate texture, cioè il
*cotangent frame*.

Su una mesh animata è corretto **per costruzione**: `fragPosition` esce dal
vertex shader dopo `matModel`, quindi le derivate lavorano sulle posizioni già
deformate dallo scheletro. Non esiste una tangente da aggiornare, perché non
esiste una tangente.

**Ovunque, non solo sugli animati.** Due percorsi che fanno la stessa cosa in
modi diversi divergono in silenzio, ed è già successo in questo progetto: lo
sfalsamento della proiezione, applicato all'albedo e dimenticato sulla normal
map, ha lasciato per settimane il rilievo del tetto fuori posto rispetto alle
sue scandole.

### Cosa questo lavoro NON risolve

Il design presentato il 9 settembre affermava che le derivate coprono il caso
che `BuildTangents()` lascia scoperto — le **UV degeneri**, dove la tangente
resta nulla e il rilievo sparisce. **È falso, e va corretto qui prima che
diventi una motivazione su cui qualcuno conta.**

Se un triangolo ha UV degeneri, `dFdx(fragTexCoord)` e `dFdy(fragTexCoord)`
sono nulli, il determinante è zero e la terna esce indefinita. Il ripiego
obbligatorio è lo stesso di oggi — tornare alla normale del vertice — solo su
una condizione diversa. Le derivate **spostano** il problema dal vertice al
frammento; non lo risolvono.

## Il cambiamento

Un solo blocco di codice. `SurfaceNormal()` oggi legge `fragTangent`, lo
raddrizza rispetto alla normale con Gram-Schmidt e ricava la bitangente da
`cross(n, t) * fragTangent.w`. Dopo:

```glsl
vec3 dp1 = dFdx(fragPosition), dp2 = dFdy(fragPosition);
vec2 du1 = dFdx(fragTexCoord), du2 = dFdy(fragTexCoord);
float det = du1.x * du2.y - du2.x * du1.y;   /* zero = UV degeneri */
```

Tre proprietà che rendono la sostituzione lecita, e nessuna delle tre è ovvia:

**Esce già in spazio mondo.** `fragPosition` e `fragNormal` sono in mondo su
*entrambi* i vertex shader — `scene.vs:41` e `scene_inst.vs:86-89` — quindi la
terna dalle derivate vive nello stesso spazio della normale con cui viene
composta. Nessuna conversione, e nessuna differenza fra percorso normale e
instanziato.

**I due rami proiettati non si toccano.** `NormaleProiettata()` costruisce già
la sua terna dagli assi della proiezione e non ha mai letto `fragTangent`.
Resta identica, riga per riga.

**Il ripiego resta `return n`**, su una condizione diversa: non più «la
tangente è nulla» ma «il determinante è nullo».

### Il rischio dichiarato: i triangoli sotto il pixel

Le derivate sono per quad di 2×2 frammenti. Su un triangolo grande come mezzo
pixel — l'erba e le foglie a 260 m — la terna può diventare rumorosa da un
fotogramma all'altro, dove la tangente interpolata di oggi è stabile.

Il rischio **non colpisce tutta la scena**, e capire perché dice anche dove
guardare. I materiali senza normal map vera ricevono da `light.c` quella
piatta, cioè `ts = (0,0,1)`, e vale l'identità

    mat3(t, b, n) * (0,0,1) == n

**qualunque** siano `t` e `b`. Terna spazzatura, risultato identico. Il rumore
può quindi comparire solo dove c'è una normal map vera **e** il triangolo è
minuscolo: i prop Poly Haven — erba, cespugli, massi, sottobosco — a distanza.

**La mitigazione non si scrive, e la ragione va agli atti.** Commutare la terna
sulla distanza vorrebbe una soglia, e una soglia va tarata contro una geometria
concreta: quella di oggi è mezza stilizzata e destinata a essere sostituita
(domanda B, domanda C), quindi qualunque numero scritto ora andrebbe rifatto
dopo. E soprattutto: lo sfarfallio dei triangoli sub-pixel non è un problema
della terna, è il problema che risolvono LOD e impostori — cioè la domanda B.
Un cerotto qui resterebbe in `scene.fs` a confondere chi legge anche dopo che B
l'avrà reso inutile.

Quello che si fa invece: **confronto a pixel su due inquadrature lontane** col
binario strumentato, prima di chiudere. Se non si vede, non si scrive niente.

## La prova

L'impalcatura c'è già: `tools/prove/normalmap.c` rende su GPU dentro una
`RenderTexture2D` di 64×64, legge il canale rosso al centro e lo confronta con
valori **calcolati a mano prima di guardarli**. Esce 77 senza contesto OpenGL.

### Perché la prova di oggi non morderebbe

**È cieca al verso della bitangente, per due ragioni indipendenti.** Le tre
normal map che usa sono `(128,128,255)`, `(191,128,238)` e `(64,128,238)`: il
canale verde è sempre 128, cioè `ts.y ≈ 0`, quindi la bitangente non entra mai
nel conto. E anche se entrasse, il sole è `(0.6, 0.8, 0.0)` mentre la
bitangente di quel quadrato sta su ±Z — prodotto scalare nullo comunque.

La dimostrazione sta dentro il file. `Quadrato()` dichiara a mano `w = +1`, che
dà `b = cross(n,t) = (0,0,-1)`. `QuadratoIndicizzato()` riceve da
`BuildTangents()` `w = -1`, cioè `b = (0,0,+1)` — quello vero, perché la v
cresce lungo +Z. **Due bitangenti opposte, e tutte e due passano con 129.**

Conseguenza: il sabotaggio che il design prescrive — *invertire la bitangente
derivata deve far fallire il confronto* — **oggi non fallirebbe**. La prova va
estesa prima di poter essere usata come garanzia. È la stessa famiglia di
difetti che `docs/06` elenca, e stavolta è stata trovata leggendo il file
invece che sbattendoci contro.

### I due casi nuovi

Sole `(0, 0.8, 0.6)`, albedo 100/255, ombre spente, formula
`AMBIENT + SUN * max(dot(n, sole), 0)` con `AMBIENT = 0.45` e `SUN = 0.85`.

**a) Perturbazione lungo la bitangente.** `PixelNormale(128, 191, 238)`, cioè
`ts = (0, 0.5, 0.866)`, sul quadrato esistente (`t = +X`, `b = +Z`, `n = +Y`).

| terna | normale risultante | diff | luce | rosso atteso |
|---|---|---|---|---|
| corretta | `(0, 0.866, 0.5)` | 0,993 | 1,294 | **129** |
| bitangente rovesciata | `(0, 0.866, -0.5)` | 0,393 | 0,784 | **78** |

Cinquantuno livelli di distacco: il sabotaggio morde.

**b) Quadrato con le UV ruotate** — u lungo +Z, v lungo +X, stessa geometria e
stessa normal map `(191,128,238)` del caso esistente. Serve contro un difetto
che nessun caso attuale vede: un'implementazione che restituisse `t = (1,0,0)`
fisso passerebbe tutto, perché in ogni quadrato del file la u cresce già lungo
+X.

| terna | normale risultante | diff | rosso atteso |
|---|---|---|---|
| corretta (`t = +Z`) | `(0, 0.866, 0.5)` | 0,993 | **129** |
| tangente inchiodata a +X | `(0.5, 0.866, 0)` | 0,693 | **104** |

### Cosa deve restare invariato

I tre casi esistenti — **113**, **129**, **78** — devono dare gli stessi
numeri. Le derivate su `Quadrato()` producono `t = +X` e `b = +Z`, che è la
terna vera, e quei tre valori non dipendono dalla bitangente. Se cambiano, il
cambiamento ha toccato qualcosa che non doveva.

### La prova resta su mesh statica, e non è un ripiego

Non esiste in gioco un personaggio con una normal map vera su cui guardare, e
non esisterà finché la domanda C non sarà chiusa. Ma se la terna dalle derivate
concorda con quella dalle tangenti sullo statico, **sull'animato segue per
costruzione**: `dFdx(fragPosition)` legge posizioni già deformate dallo
scheletro, e non c'è nessun altro ingrediente che l'animazione possa sporcare.

Va scritto nella prova stessa, o qualcuno cercherà una verifica in gioco che
non può ancora esistere.

## La misura del costo

Le tangenti si pagavano **una volta al caricamento**; le derivate si pagano
**per frammento**. È il rischio principale di questo lavoro, e va misurato, non
dichiarato gratis.

**Il passaggio d'ombra non entra nel conto.** In `main()` ci sono due uscite
anticipate — `depthOnly == 1 && alphaCut <= 0.0` all'inizio, e `depthOnly == 1`
subito dopo il ritaglio — ed entrambe stanno *prima* della riga che costruisce
la normale. `SurfaceNormal()` non viene mai chiamata nelle due cascate. Il
numero da isolare è quello del **solo passaggio principale**.

**Il banco è quello di sempre**, e va ripetuto perché è l'unica cosa che rende
confrontabili le misure fra lavori diversi: copia dei sorgenti fuori dal repo,
`GS_PLAY` forzato in `GameInit()` **e** in `GameNewWorld()`, `g->player.yaw`
che ruota per campionare tutte le direzioni, 75 secondi, stesso tracciato.

**Tre passi, e l'ordine conta:**

1. **Il rumore di fondo.** Due giri del binario *non modificato*. Se due giri
   identici differiscono già del 3%, una soglia del 5% distingue poco e va
   alzata. Senza questo numero la soglia è un gesto, non una misura.
2. **La soglia, dichiarata prima di guardare il risultato: +5% sul passaggio
   principale**, da correggere verso l'alto se il rumore del passo 1 lo impone.
   Va scritta qui dentro *prima* di eseguire il passo 3. La ragione sta
   nell'intestazione di `normalmap.c`: i valori attesi si calcolano prima di
   guardarli, altrimenti la prova fotografa ciò che il codice fa. Una soglia
   scelta dopo aver visto il numero fa esattamente la stessa cosa.
3. **La misura del cambiamento.** Stesso percorso, binario nuovo. Il numero va
   nella spec qualunque sia, anche se è brutto.

### Il banco, e come si cattura il tempo — 2026-09-09

Due copie dei sorgenti fuori dal repo, dai commit `04cbe85` (terna
dall'attributo) e `283da0e` (terna dalle derivate), con la **stessa identica**
strumentazione: le due copie differiscono solo in `assets/shaders/scene.fs`, e
i due eseguibili compilati sono byte per byte identici. Se differissero in
qualcos'altro la misura non varrebbe niente.

Lo strumento è `glFinish()` più orologio a parete attorno al **solo** blocco
`BeginMode3D(g->cam) … EndMode3D()` di `DrawScene()`. `glFinish()` serializza
la GPU e distorce il tempo assoluto del fotogramma: non importa, perché lo
distorce allo stesso modo nelle due copie, e la domanda è di quanto **cambia**
il passaggio, non quanto vale. Il passaggio d'ombra resta fuori dal cronometro,
per la ragione detta sopra.

Il resto del banco: vsync e `SetTargetFPS()` tolti, finestra nascosta,
`GS_PLAY` forzato, il giocatore fermo al punto di partenza e la visuale che
ruota di 0,013 rad a ogni fotogramma. Niente input e niente simulazione: così
l'insieme degli yaw visitati è lo stesso in tutte le esecuzioni e non dipende
da quanto è veloce la macchina. Settantacinque secondi, macchina WSL2 con
WSLg, driver `d3d12` su NVIDIA RTX 5070.

### Il rumore di fondo, e la soglia dichiarata

**Il rumore di fondo.** Tre giri della copia `prima`, la stessa, non toccata:

| giro | fotogrammi | passaggio principale |
|---|---|---|
| 1 | 9409 | **2,486 ms** |
| 2 | 9218 | **2,528 ms** |
| 3 | 9257 | **2,527 ms** |

Media 2,514 ms, escursione fra il minimo e il massimo 0,042 ms, cioè
**1,7%**.

**La soglia: +5% sul passaggio principale**, cioè **2,64 ms**. Resta quella
proposta, e il rumore la giustifica: 1,7% di rumore sta tre volte sotto il 5%,
quindi la soglia distingue davvero qualcosa invece di fotografare il caso. Se
il rumore fosse stato del 4% questa riga direbbe un altro numero.

**Questo paragrafo è scritto e committato PRIMA di lanciare la copia `dopo`.**
È ciò che rende la soglia una previsione invece di una descrizione: una soglia
scelta dopo aver visto il risultato fa esattamente quello che l'intestazione di
`normalmap.c` vieta ai valori attesi.

### La rete, progettata e non scritta

Se il passo 3 sfora la soglia: una uniform intera che accende le derivate solo
dove servono — le mesh animate — e lascia tutto il resto alle tangenti
interpolate.

**Non viaggia per lotto**, e la ragione è una scoperta che accorcia il lavoro:
i personaggi non passano dall'instancing. I lotti di `instancing.c` sono i
prop, e nessun prop è animato; giocatore e NPC si disegnano dal percorso non
instanziato, in `game.c:574-579`. L'interruttore vive quindi accanto agli altri
stati di `light.c`, si accende intorno ai personaggi e si rispegne subito dopo
— come la soglia dell'alfa nei lotti, perché uno stato lasciato acceso lo paga
chi viene dopo, qui in frammenti su tutta la scena.

**Il codice non si scrive finché la misura non lo chiama**, per la stessa
ragione per cui non si scrive la mitigazione del rumore: sarebbe una
mitigazione speculativa contro un difetto mai visto, e questo repo misura prima
di scrivere.

### Un dettaglio agli atti

Sulle foglie il `discard` del ritaglio rende il flusso non uniforme dentro il
quad 2×2, quindi le derivate al bordo del ritaglio sono approssimate. È la
stessa approssimazione che le GPU fanno già oggi per scegliere il livello di
mip di `texture()`, con le stesse corsie d'aiuto. **Non è una regressione
introdotta da questo lavoro**, ed è il motivo per cui non compare fra i rischi:
sta qui perché altrimenti sembrerà un difetto nuovo a chi lo incontrerà.

## Fuori ambito

Se le derivate reggono, `BuildTangents()`, l'attributo `vertexTangent` e il
varying `fragTangent` diventano codice morto, e toglierli restituirebbe quattro
float di varying su tutta la scena.

**Non si tolgono qui, e la ragione non è prudenza: rovinerebbero la misura.**
Il numero del passo 3 deve rispondere a una domanda sola — *quanto costano le
derivate?* Se nello stesso cambiamento sparissero quattro float di varying, il
delta misurato sarebbe la somma di due effetti di segno opposto e non
attribuibile a nessuno dei due. La soglia dichiarata al passo 2 diventerebbe
carta straccia.

E c'è un secondo motivo: `tools/prove/normalmap.c` **dichiara le tangenti a
mano** proprio per rendere il valore atteso calcolabile con carta e penna.
Quella prova è lo strumento con cui si misura questo lavoro. Cambiare lo
strumento nello stesso passo della cosa che deve misurare è il modo classico di
non provare niente.

La rimozione diventa un lavoro suo, che parte da una condizione precisa — *il
percorso nuovo è provato in gioco su un personaggio con normal map vera* — e
che a quel punto potrà misurare il proprio guadagno pulito. Tocca la
disposizione degli attributi in `instancing.c`, che è delicata.

## Come si verifica che regga

```bash
make            # Linux e Windows, zero avvisi
make prove      # normalmap.c compreso; 77 = saltata, niente contesto GL
```

Più le tre cose che `make prove` non può fare, e che chiudono il lavoro:

- il **sabotaggio**: invertire la bitangente derivata deve far fallire il caso
  (a), e inchiodare la tangente a `(1,0,0)` deve far fallire il caso (b);
- il **confronto a pixel** su due inquadrature lontane, per il rumore sui
  triangoli sotto il pixel;
- la **misura** in tre passi, col numero scritto qui dentro.

## Documenti collegati

- `docs/06-stato-e-prossimi-passi.md` — la domanda F, e il vincolo delle
  tangenti sulle mesh animate
- `docs/01-architettura.md`, sezioni *Normal map* e *Materiali proiettati* —
  come funziona oggi la terna, e i due difetti di correttezza trovati leggendo
- `docs/superpowers/specs/2026-09-06-materiali-triplanari-design.md` — il
  precedente dei due percorsi che divergono in silenzio
