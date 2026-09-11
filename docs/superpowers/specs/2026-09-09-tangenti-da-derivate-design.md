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
normale piatta di riserva installata da `FitFlatNormal()`, quindi
`SurfaceNormal()` restituisce la normale del vertice a meno di quattro
millesimi. *A meno di*, non *esattamente*: quella riserva è la texture
`(128,128,255)` di `light.c:326`, che decodifica in `(0,0039, 0,0039, 1)` e non
in `(0,0,1)`. La differenza sta sotto la soglia del visibile, ma non è zero —
vedi *Il rischio dichiarato*.

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

### La regressione vera: fp32, in campo vicino e lontano dall'origine

È **l'unica regressione** che questo ramo introduce — le UV degeneri erano
scoperte anche prima, i triangoli sotto il pixel sono un rischio dichiarato e
misurato — e cade proprio sul caso per cui il ramo esiste: **un volto a uno o
due metri.** Va agli atti qui, non riparata ora.

**Perché succede.** Le derivate si prendono su `fragPosition`, che è una
posizione **assoluta in metri-mondo** e arriva a `WORLD_SIZE` = 4096
(`src/config.h`). In fp32 un ulp vale `x · 2⁻²³`: a `x ≈ 3000` sono circa
`3,6·10⁻⁴ m`. Con `fovy = 70°` il passo di mondo per pixel a distanza *d* vale
`2 · tan(35°) · d / righe`, cioè `d · 1,3·10⁻³ m` su 1080 righe e
`d · 1,9·10⁻³ m` sui 720 di `config.h` — **più fitta è l'immagine, peggio è.**
A un metro dalla superficie il passo vale quindi tre o quattro ulp; a mezzo
metro, uno e mezzo o due.

`dFdx(fragPosition)` è la differenza di **due varying già arrotondati**: il suo
errore assoluto è dell'ordine dell'ulp, su un passo che di ulp ne conta tre o
quattro. In campo vicino e lontano dall'origine la terna può quindi arrivare
con un **errore relativo dell'ordine del 15–30%**, dove la tangente interpolata
di prima non aveva questo problema — l'interpolazione di un attributo non
sottrae due numeri grandi quasi uguali.

**Chi ne risente.** I prop Poly Haven a cui ci si accosta; `crypt.gltf`, dove
dentro la cripta le pareti stanno sotto il metro; e `BUILD_KEEP` /
`BUILD_STATUE`, che in `src/world.c:295-296` hanno mode 0 e `uvVere = true` e
quindi passano da `SurfaceNormal()` con una normal map vera. **Il terreno no:**
riceve la normale piatta.

**La cura vera, e non è un ritocco.** Costruire le derivate su una posizione
**relativa alla camera**, calcolata nel vertex shader: lì la sottrazione
`posizione − viewPos` avviene una volta per vertice e il varying che arriva al
fragment porta già numeri piccoli, dove l'ulp è quello del metro e non quello
del chilometro. Tocca tutti e due i vertex shader e il varying che condividono:
è un lavoro suo.

**La verifica costa quasi niente**, col banco già descritto più avanti: **lo
stesso masso a un metro, una volta vicino all'origine e una volta all'angolo
lontano della mappa.** Se la terna degrada col modulo della posizione, è questo
e non altro.

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
piatta, e vale l'identità

    mat3(t, b, n) * (0,0,1) == n

**qualunque** siano `t` e `b`. Ma `ts` non è `(0,0,1)`: la riserva di
`light.c:326` è la texture `(128,128,255)`, e `128/255 · 2 − 1 = 0,0039`.
L'identità è vera in matematica e falsa di quattro millesimi in aritmetica a
8 bit — **la terna filtra dentro anche lì**. E si misura: fra la copia con le
tangenti e quella con le derivate, il **9,25% dei pixel** dell'inquadratura di
prova differisce di almeno un livello, e sono quasi tutti nevaio: cioè proprio
i pixel dove l'identità qui sopra prometteva che non potesse cambiare niente. Un livello su 255 sta
sotto la soglia del visibile e nessuna conclusione di questo lavoro cambia; ma
chi userà «terna spazzatura, risultato identico» per saltare un controllo deve
sapere che è un'approssimazione buona, non un teorema.

Il rumore vero e proprio può comparire solo dove c'è una normal map vera **e**
il triangolo è minuscolo: i prop Poly Haven — erba, cespugli, massi,
sottobosco — a distanza.

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

#### Misurato il 2026-09-09: lo sfarfallio c'è, ed è sessanta volte sotto il tremolio

Non è stato guardato, è stato **contato**: lo sfarfallio è varianza temporale.
Stesso binario strumentato, modalità alternativa, sessanta fotogrammi
consecutivi presi con `LoadImageFromScreen()`, differenza media assoluta fra
fotogrammi consecutivi sul canale della luminanza.

**Dove.** `x = 2350`, `z = 2300` (quota dal terreno), sguardo a **60°**,
inclinazione **−0,10 rad**: il nevaio a nord-est. Nel campo visivo ci sono
**51 prop Poly Haven con normal map vera — 37 massi e 14 cespugli — da 17 a
136 m**, ventiquattro dei quali oltre i 90 m, e nessun albero che li copra.

**Perché lì e non in una foresta.** In foresta la scena è fatta di alberi
KayKit, che una normal map vera non ce l'hanno: la prova avrebbe misurato
terreno e cielo.

**E perché non «a 150–260 m» come diceva il piano.** Nessun prop Poly Haven
viene disegnato oltre i 140 m: `PropMaxDist()` taglia i massi e i tronchi a
140, i ceppi a 100, i cespugli e l'erba a 80, il sottobosco fra 40 e 60. Più in
là la fascia è **quasi** vuota di ciò che si voleva guardare — e «quasi» va
detto, perché la prima stesura di questa sezione scriveva «vuota per
costruzione», e quello è falso. Ci stanno dentro almeno due cose:

- la **cripta**. `PROP_CRYPT` non compare in `gPropDetail`, quindi
  `PropMaxDist()` (`src/world.c:1097`) le assegna il `default: 400.0f`.
  `crypt.gltf` ha una normal map vera, 63 127 triangoli, e `projMode == 0`:
  passa esattamente da `SurfaceNormal()`;
- il **maschio del forte e la statua**. `BUILD_KEEP` e `BUILD_STATUE` in
  `gBuildMat` (`src/world.c:295-296`) hanno mode 0 e `uvVere = true`: **non**
  passano da `NormaleProiettata()` come le altre murature, passano da
  `SurfaceNormal()`. `modular_fort_01.gltf` porta tre normal map,
  `statua.gltf` 27 739 triangoli, e case e torri si vedono fino a 400 m.

Attorno alla cripta ci sarebbe quindi stato il caso che il piano chiedeva: una
statua da 27 739 triangoli vista a 200 m occupa nove pixel di lato scarsi, cioè
circa **360 triangoli per pixel**. Il nevaio non era l'unico posto possibile.
Era però il posto migliore, e la ragione è di copertura, non di esistenza:
**dà la stessa densità di triangoli per pixel su 51 prop** invece che su un
singolo manufatto in un singolo punto del mondo, e su una geometria — i massi —
che il gioco disegna a migliaia. La deviazione dal piano resta giustificata; ma
è una scelta argomentata, non un teorema, e chi ripeterà la misura sa adesso
dove sta l'altro caso.

Non è una rinuncia, perché **la distanza non è la condizione vera**: la
condizione è il triangolo sotto il pixel, e `rock.gltf` ha 59 066 triangoli su
2,2 m. A 100 m un masso occupa una dozzina di pixel: sono centinaia di
triangoli per pixel. La soglia oltre cui i suoi triangoli scendono sotto il
pixel è **6,5 m**, non 150. Con questi asset il rischio, se esistesse, si
vedrebbe da qualunque distanza.

**Il passo.** La camera avanza di **1 cm a fotogramma** lungo la direzione
dello sguardo — 59 cm in tutta la sequenza — dopo quaranta fotogrammi di
riscaldamento. Il movimento serve: ferma, la scena darebbe fotogrammi identici
e varianza zero in tutte e due le copie, e la prova non morderebbe. In avanti e
non di lato perché il flusso di pixel di una traslazione frontale è minimo
verso il centro dello schermo, dove stanno i massi lontani: così la differenza
fra fotogrammi non è dominata dal movimento.

**I due numeri.** La misura è deterministica — due esecuzioni della stessa
copia danno la stessa cifra fino alla quarta decimale.

| | differenza media fra fotogrammi | pixel che cambiano | picco |
|---|---|---|---|
| prima | **0,23933** livelli | 18,165% | 115,30 |
| dopo  | **0,24000** livelli | 18,218% | 116,23 |

**+0,25%: sei decimillesimi di livello su 255 — e non è rumore di misura.** La
stessa copia, rilanciata, si ripete entro ±0,0001 livelli; lo scarto fra le due
copie è +0,0006, sei volte tanto. L'effetto c'è, ed è misurato: la domanda è
quanto sia piccolo, non se esista.

**E non è concentrato sui massi.** Una mappa per pixel della varianza
temporale, confrontata fra le due copie, dice dove sta quel poco. La maschera
non è disegnata a mano: sono i pixel in cui i due shader danno risultati
diversi, cioè dove la normal map è vera. Stringendola, l'effetto **cresce in
modo monotono** — +0,4% sugli 85 252 pixel che differiscono di almeno un
livello, +1,7% sui 114 che ne differiscono di due, +3,7% sui 39 che ne
differiscono di cinque. Una rampa così è la firma di un effetto reale, non di
un arrotondamento casuale, e va letta come tale. Sui 114 pixel più sensibili —
i bordi dei massi, dove la terna conta davvero — la varianza passa da 3,364 a
3,421 livelli: sei centesimi di livello aggiunti a un tremolio da movimento che
è già sessanta volte più grande, e a sua volta invisibile. Quattro quinti
dell'aumento totale stanno comunque **fuori** da quelle maschere, sparsi sul
nevaio: nessun difetto localizzato.

**Conclusione: lo sfarfallio dei triangoli sotto il pixel esiste, è stato
misurato, e sta circa sessanta volte sotto il tremolio da movimento e ben
sotto la soglia del visibile. Non si scrive nessuna mitigazione** — che era già
la decisione presa qui sopra per ragioni di progetto, e adesso ha anche un
numero. La differenza rispetto a «non esiste» non è pignoleria: dice a chi
rimisurerà con asset diversi che questo banco un effetto lo distingue, e quanto
grande era quello di oggi.

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

### Il risultato: le derivate non costano, e il perché non è quello che sembra

Sei giri per copia, gli ultimi tre **alternati** — `prima`, `dopo`, `prima`,
`dopo` — perché nella prima tornata tutti i giri di `prima` erano venuti prima
di tutti quelli di `dopo`, e la macchina deriva verso l'alto man mano che si
scalda: fra il primo e l'ultimo giro di `prima` ci sono 4,1%, cioè più del
rumore dichiarato. L'alternanza è ciò che rende il confronto sano.

| giro | prima | dopo |
|---|---|---|
| 1 | 2,486 ms | 2,422 ms |
| 2 | 2,528 ms | 2,443 ms |
| 3 | 2,527 ms | 2,471 ms |
| 4 *(alternato)* | 2,550 ms | 2,481 ms |
| 5 *(alternato)* | 2,553 ms | 2,485 ms |
| 6 *(alternato)* | 2,588 ms | 2,490 ms |
| **media** | **2,539 ms** | **2,465 ms** |

**Differenza: −2,9%.** Le tre coppie alternate, prese una per una, danno
−2,7%, −2,7% e −3,8%: sempre lo stesso segno, sempre della stessa taglia.

La soglia era +5%. **Rispettata, e con il segno opposto a quello temuto: il
passaggio principale è più veloce di prima.**

**L'ordine non può aver fabbricato questo segno, e l'argomento è più forte
dell'alternanza.** La macchina deriva verso l'alto scaldandosi. Nella prima
tornata i tre giri `dopo` sono girati *dopo* tutti i `prima`, cioè a macchina
più calda, e sono comunque risultati più veloci; dentro ogni coppia alternata
il `dopo` è il secondo dei due, di nuovo il più caldo. In tutte e sei le
esecuzioni **la deriva termica spinge contro il segno osservato**. Il −2,9% è
quindi una stima conservativa — un minorante del guadagno vero — non un
artefatto dell'ordine.

**E va detto che il «rumore 1,7%» del passo 1 non era una stima di
ripetibilità.** Erano i primi tre punti di una rampa monotona — la macchina che
si scalda — non tre campioni indipendenti attorno a una media. La dispersione
vera dell'ambiente è quel 4,1% fra il primo e l'ultimo giro di `prima`, che sta
a **0,8 volte** la soglia dichiarata: con un risultato vicino al +4% questo
banco non avrebbe potuto decidere niente, e sarebbe servito un protocollo
diverso — alternanza fin dal primo giro, o un plateau termico prima di
misurare. Il risultato è utilizzabile perché cade lontano dalla soglia e dalla
parte giusta, non perché il banco fosse preciso.

**Ma il numero non risponde alla domanda che sembra.** Nella copia `dopo`
`fragTangent` non viene più letto da nessuna parte del fragment shader: resta
dichiarato, e diventa un varying morto che il compilatore GLSL può togliere —
con lui l'interpolazione di quattro float per frammento su tutta la scena. Il
−2,9% è quindi la **somma** di due effetti di segno opposto: le derivate che
costano, e un varying che sparisce da solo. La sezione *Fuori ambito* voleva
tenerli separati non rimuovendo `BuildTangents()` nello stesso passo; non
bastava, perché il ramo morto se lo porta via il driver senza chiedere
permesso.

**E il credito è almeno grande quanto dichiarato, probabilmente di più.** Con
l'output morto cade anche l'ALU del vertex shader che lo calcola, e con ogni
probabilità il prelievo dell'attributo `vertexTangent` stesso: sedici byte per
vertice, su mesh da decine di migliaia di triangoli. Il costo vero delle
derivate sta quindi nascosto sotto un credito di taglia ignota ma non piccola.

**L'esperimento che li separa, scritto qui perché chi verrà dopo lo trovi
pronto:** rilanciare la copia `dopo` con `fragTangent` **tenuto vivo** da una
lettura inerte e non ottimizzabile via — un contributo che il compilatore non
possa dimostrare nullo, per esempio pesato da una uniform che a runtime vale
zero — e confrontarla con la copia `dopo` di oggi. Quattro giri di banco,
alternati.

**Non si esegue ora, e questa è una decisione presa, non una dimenticanza.** Il
banco è stato cancellato; la domanda che il piano poneva — *sfora la soglia?* —
ha già risposta, negativa e per giunta conservativa; e nessuna decisione cambia
col numero isolato, perché la mitigazione non si scrive in nessuno dei due
casi. L'esperimento diventa il **primo passo del lavoro che toglierà
`BuildTangents()`**, che è il lavoro di cui quel numero misura davvero il
guadagno. Sta scritto in *Fuori ambito*, dove chi lo aprirà lo troverà.

Quello che il numero dice con certezza, e che è la domanda che contava: **il
cotangent frame non fa sforare il budget del fotogramma, in nessuna delle sei
esecuzioni.**

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
mip di `texture()`, con le stesse corsie d'aiuto.

**La classe di esposizione è identica; la conseguenza no, e la differenza va
detta.** Una derivata di UV approssimata sceglie un livello di mip sbagliato, e
un mip sbagliato al bordo di una foglia non si vede. Una derivata di posizione
approssimata dà una *terna* sbagliata, cioè una normale sbagliata, e quella si
vede: cambia come il frammento prende la luce. Quindi non è un'esposizione
nuova — quei frammenti derivavano già — ma **è una conseguenza nuova**, e chi
dovesse vedere bordi di foglia illuminati storti sappia che è qui che va
guardato. Sta agli atti e non fra i rischi perché il ritaglio colpisce una
fascia di frammenti larga un pixel su asset che una normal map vera non ce
l'hanno; il giorno in cui una fogliame fotogrammetrica ne porti una, questa
riga va riletta.

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

**Il suo primo passo è già scritto**, ed è l'esperimento descritto in *Il
risultato*: rilanciare la copia con le derivate tenendo vivo `fragTangent` con
una lettura inerte, quattro giri di banco alternati. Separa il costo delle
derivate dal credito del varying morto, e va fatto **prima** di togliere
qualunque cosa — è la linea di base rispetto a cui la rimozione misurerà il
proprio guadagno. Non è stato fatto in questo lavoro perché nessuna sua
decisione dipendeva da quel numero; dipende invece interamente da quel numero
il lavoro di rimozione.

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
