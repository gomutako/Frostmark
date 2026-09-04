# 03 — Asset pubblici

## Premessa: il gioco non ne ha bisogno

Frostmark funziona con **zero file binari**. Texture, terreno, edifici, alberi e
personaggi sono generati a runtime da primitive (`GenMeshCylinder`, `GenMeshCone`,
`GenMeshSphere`, `GenMeshCube`) e da texture procedurali (`MakeGrainTexture`).
Questo è deliberato: un progetto didattico che si scarica e compila senza
dipendere da un CDN, e che non ha problemi di licenza da nessuna parte.

Gli asset esterni sono quindi un **innesto opzionale**. Questo documento spiega
dove prenderli restando nel pubblico dominio e dove esattamente agganciarli.

Stato degli innesti nel codice:

| Asset | Come si attiva |
|---|---|
| `assets/heightmap.png` | si passa al baker: `./baker --heightmap ... --forza` (vedi `docs/02`) |
| `assets/textures/grass.png` | rilevato in automatico (`LoadTerrainTexture`) |
| `assets/models/*.glb` | rilevati in automatico (`LoadExtProps`), texture inclusa |
| `assets/models/player.glb` | rilevato in automatico (`PlayerLoadModel`), con animazioni |
| `assets/models/npc_*.glb` | rilevati in automatico (`EntitiesLoadModels`) |
| `assets/fonts/ui.ttf`, `title.ttf` | rilevati in automatico (`UILoadFonts`) |
| `assets/audio/*` | **richiede modifiche a `main.c`** (sezione 6) |

I primi quattro bastano copiare al posto giusto: se il file manca, il gioco usa
la versione procedurale e non cambia niente.

---

## Fonti CC0 affidabili

CC0 significa rinuncia al diritto d'autore: uso commerciale libero, nessuna
attribuzione obbligatoria (resta comunque buona educazione citare l'autore).

| Fonte | Cosa offre | Licenza |
|---|---|---|
| **kenney.nl/assets** | modelli low-poly (78-114 triangoli), icone UI, font, suoni — download diretto, l'unica sorgente davvero adatta a questo progetto | CC0 |
| **ambientcg.com** | texture PBR tileable (erba, roccia, neve, legno, pietra) | CC0 |
| **polyhaven.com** | texture, HDRI, modelli fotogrammetrici — **troppo pesanti per i prop**, vedi sotto | CC0 |
| **quaternius.com** | modelli low-poly stilizzati: alberi, edifici, personaggi animati — download via Google Drive, non automatizzabile | CC0 |
| **opengameart.org** | archivio misto — **filtrare per CC0**, molti asset sono CC-BY o GPL | varia |
| **freesound.org** | suoni ambientali — **filtrare per CC0** | varia |
| **fontlibrary.org**, **fonts.google.com** | font (OFL) | OFL/varia |

Regola pratica: su OpenGameArt e Freesound la licenza va verificata asset per
asset. Su Kenney, ambientCG, Poly Haven e Quaternius è CC0 per tutto il catalogo.

**Attenzione**: *non* usare asset estratti da Skyrim o da altri giochi
commerciali. Non sono pubblici, e non lo diventano perché il gioco è vecchio.

---

## Pacchetti consigliati per Frostmark

Lo script `tools/fetch_assets.sh` elenca i download consigliati e prepara le
cartelle (non scarica automaticamente: gli URL cambiano e conviene che sia una
scelta consapevole).

```
assets/
  heightmap.png          terreno esterno (vedi doc 02)
  textures/
    grass.png                                    da ambientCG o Poly Haven
  models/
    tree.glb  pine.glb  rock.glb  bush.glb  herb.glb    dal Nature Kit di Kenney
    player.glb                                          personaggio animato, da KayKit
  fonts/
    ui.ttf  title.ttf                            da Google Fonts (OFL)
  audio/
    ambient_wind.ogg  hit.wav  step.wav          da Kenney / Freesound
```

### Download automatico dei modelli

```bash
./tools/fetch_assets.sh models
```

Scarica il **Nature Kit di Kenney**, verifica che `License.txt` dichiari CC0,
estrae cinque modelli, li rinomina come li cerca `world.c` e aggiunge le righe in
`assets/CREDITS.md`. L'URL non è inchiodato nello script: contiene un hash che
cambia a ogni aggiornamento del pacchetto, quindi viene letto dalla pagina.

![Modelli Kenney in gioco](img/04-modelli-kenney.png)

**Perché Kenney e non Poly Haven.** La ragione era che Frostmark disegnava
centinaia di prop per frame con `DrawModelEx`, uno alla volta. **Non è più
vera**: da settembre 2026 i prop si disegnano a lotti (vedi *Instancing* in
`docs/01`), e i triangoli hanno smesso di essere il problema.

Misurato sostituendo *tutti* i prop con `rock_07` di Poly Haven, 14.844
triangoli l'uno: il carico passa da ~118.000 a **7,3 milioni di triangoli per
fotogramma** e la scena da 4,6-5,1 a 5,0-5,3 ms. Sessantadue volte i triangoli,
tre decimi di millisecondo.

Quello che rende inutilizzabile un asset non è quindi il conteggio in sé, ma
quanto è *fuori scala* — e alcuni lo sono parecchio:

| asset Poly Haven | triangoli | vertici/mesh | alfa | si usa? |
|---|---|---|---|---|
| `rock_07` | 14.844 | 7.914 | opaco | sì, ma è largo 32 cm |
| `namaqualand_boulder_04` | 59.066 | 30.165 | opaco | **sì — è quello in uso** |
| `namaqualand_boulder_02` | 97.964 | 53.437 | opaco | sì |
| `boulder_01` | 66.122 | **67.042** | opaco | **no**: oltre il tetto dei 16 bit |
| `nettle_plant` | 31.304 | — | MASK | serve una specie adatta al bioma |
| `fir_sapling` | 433.021 | — | opaco | è una piantina, non un albero |
| `fir_tree_01` | **6.982.937** | — | BLEND | **no**: 478 MB di sola geometria |

**Il tetto vero non è il conteggio dei triangoli: è quello dei vertici.** Il
`Mesh` di raylib 5.5 tiene gli indici in `unsigned short`, quindi oltre **65.535
vertici per mesh** li tronca. Avvisa con una riga in mezzo a centinaia, e il
risultato non è un errore ma un difetto visivo: gli indici si avvolgono e
nascono triangoli che attraversano l'oggetto da parte a parte.

`boulder_01` è la trappola perfetta — 66.122 triangoli sembrano innocui, ma
sono 67.042 vertici, millecinquecento oltre il limite. `LoadExtProps()` ora
scarta i modelli oltre il tetto con un avviso esplicito e torna alla primitiva
procedurale: meglio una sfera onesta di un masso sfregiato. **Quando si sceglie
un asset si guarda il conteggio dei vertici, non quello dei triangoli.**

L'abete da solo è 59 volte l'intera scena attuale, alla risoluzione di texture
più bassa. Non è un asset da decimare: ridurlo a qualcosa di usabile sarebbe
rifarlo.

Due cose bloccano oggi la vegetazione, e nessuna è di prestazioni. **L'alfa**:
ogni pianta del catalogo è `MASK` o `BLEND` — le foglie sono ritagli su
quadrati — e `scene.fs` non gestisce l'alfa. **Le mesh multiple**: un lotto si
crea solo per i modelli a mesh singola, e nel catalogo sono l'eccezione. I
sassi, che sono opachi e a mesh singola, funzionano già.

**Nessun asset del catalogo porta le tangenti nel file.** Le calcola
`BuildTangents()` al caricamento — 1 ms per il sasso — e senza, ogni normal map
illuminerebbe storto. Poly Haven resta utile anche per le **texture**.

**Attenzione ai .glb di Kenney: vanno riparati.** I kit sono esportati con
UniGLTF (Unity), che indica come radice della scena un nodo che ha già un
genitore (`tmpParent`). La specifica glTF 2.0 lo vieta, e cgltf — il parser di
raylib — rifiuta l'intero file:

```
WARNING: MODEL: [assets/models/tree.glb] Failed to load glTF data
```

Blender e three.js sono più permissivi, per questo il problema non è noto.
`tools/glb_fix_scene.py` ripunta la scena sui nodi radice veri senza toccare
geometria, materiali o trasformazioni; `fetch_assets.sh models` lo applica da
sé. Il Survival Kit di Kenney, esportato con UnityGLTF, non ha il difetto.

**Perché il Survival Kit e non il Nature Kit.** `fetch_assets.sh models`
scarica il **Survival Kit**, che è texturizzato: un atlante 512x512 condiviso
da tutti i modelli, cioè una sola texture per l'intera foresta. Il Nature Kit
resta disponibile con `fetch_assets.sh models nature`, ma non ha texture — i
suoi colori stanno nei vertici.

**La tavolozza del Nature Kit non coincide.** È deliberatamente acida: fogliame
turchese (43, 166, 170) e tronchi salmone (204, 118, 94), anche nelle varianti
`_dark`. Sul terreno verde scuro di Frostmark stona. Due strade, entrambe
legittime:

1. schiarire e desaturare i colori di bioma in `WorldBiomeColor()`, adottando la
   tavolozza di Kenney per tutto il gioco;
2. riscrivere i `baseColorFactor` dei `.glb` con i colori usati da `DrawProp()`
   (CC0 permette la modifica; `assets/CREDITS.md` documenta la provenienza).

Il Survival Kit ha verdi più naturali ed è quello in uso, ma contiene solo
conifere: albero e pino sono due varianti dello stesso profilo, e il bosco
risulta uniforme.

---

## Punti di innesto nel codice

### 1. Texture del terreno (il più semplice, e quello che si vede di più)

Basta copiare una texture tileable in `assets/textures/grass.png`: viene caricata
da `LoadTerrainTexture()` in `world.c`, con mipmap, filtro trilineare e wrap
ripetuto. Se il file non c'è, si usa `MakeGrainTexture()` come prima.

Nota: i colori dei vertici (bioma + luce pre-calcolata) **moltiplicano** la
texture. Se la texture è molto colorata il risultato diventa cupo: in quel caso
conviene schiarire i colori di bioma in `WorldBiomeColor()` oppure usare texture
desaturate. Una texture fotografica a fuoco richiede anche di abbassare
`TERRAIN_UV_TILE` in `config.h` (metri coperti da una ripetizione, 8 di
default). Un passo avanti è il *texture splatting* (esercizio 8): la texture
attuale è unica per tutti i biomi.

### 2. Modelli al posto delle primitive

Copiare i file in `assets/models/` con questi nomi (`fetch_assets.sh models` lo
fa da sé):

| File | Sostituisce | Scala in `gExtProp` |
|---|---|---|
| `tree.glb` | l'albero (cilindro + sfera) | 4.61 → 6.5 m |
| `pine.glb` | il pino (cilindro + cono) | 3.97 → 6.8 m |
| `rock.glb` o `rock.gltf` | il sasso (sfera schiacciata) | 2.2 m di larghezza |
| `bush.glb` | il cespuglio (sfera) | 2.87 → 1.4 m di larghezza |
| `herb.glb` | l'erba curativa della quest | 4.50 → 0.9 m |
| `graveyard/crypt.glb` | la cripta (cubi + colonne) | 5.00 → 5 m |

**Le scale non si tarano più a mano.** La tabella `gExtProp` in `world.c`
dichiara *quanto deve essere grande* l'oggetto in metri — 6,5 per l'albero, 2,2
per il sasso — e `LoadExtProps()` ricava il moltiplicatore dall'ingombro vero
del modello, con `GetModelBoundingBox()`. Sostituire un asset non richiede
quindi di ricalcolare niente: il masso Poly Haven da 2,52 m entra a ×0,87, il
sasso Kenney da 0,62 m a ×3,54, e nel mondo sono grandi uguale.

Prima erano costanti scritte a mano, e un conto sbagliato lì non dava nessun
avviso: dava un albero alto tre volte tanto.

Il file può essere `.glb` o `.gltf`: i kit spediscono il primo, Poly Haven il
secondo con il `.bin` e le texture accanto. `LoadExtProps()` prova quello
dichiarato e poi l'altra estensione.

Casa e torre non sono in questa tabella: non sono modelli singoli ma
composizioni di pezzi, vedi più sotto.

### Due tranelli sulle texture

**I `.glb` di Kenney non contengono la texture.** La cercano accanto a sé, in
`Textures/colormap.png`, e senza quel file gli alberi si vedono **bianchi** —
il modello carica, il materiale no. Lo zip la porta in
`Models/GLB format/Textures/colormap.png`; c'è anche
`Models/Textures/variation-a.png`, che è una tavolozza alternativa e non il
file cercato.

**Due kit diversi hanno due `colormap.png` diversi.** raylib risolve l'immagine
rispetto alla cartella del modello, quindi due atlanti omonimi non possono
stare vicini: la cripta del Graveyard Kit sta in
`assets/models/graveyard/` con la sua `Textures/`.

### Casa e torre: un edificio è una ricetta, non un file

Nei kit CC0 gli edifici medievali non esistono come modello unico: esistono i
**pezzi** — muro, muro con porta, muro con finestra, falda, solaio — su una
griglia di celle da 1 unità, e i pezzi della torre che si impilano. `world.c`
li compone in `DrawHouse()` e `DrawTower()`:

| Edificio | Ricetta | Pezzi disegnati |
|---|---|---|
| casa bassa | 3×2 celle, muri sul perimetro, porta al centro della facciata, finestre altrove, tetto a due falde | 19 |
| casa alta | 4×3 celle su **due piani**, con solaio, tromba delle scale e una rampa per salire | 45 |
| torre | base + due piani con feritoie + coronamento + tetto | 5 |

Quale delle due esce da una data casa lo decide `HouseShapeOf()` in `world.c`, a
partire dalla **posizione**: una su tre circa è alta. È una funzione, non un
dato — il mondo cotto non cambia, e disegno, collisione e superfici calpestabili
la ricavano allo stesso modo. Se divergessero si camminerebbe su un piano che
non c'è, ed è successo davvero: la prima versione dava il vano della porta per
centrato sulla facciata, ma con pianta **pari** la porta sta a mezza cella dal
centro, e il giocatore restava fuori a sbattere contro il muro accanto.

Il pezzo della scala sale esattamente **una unità in una cella**, cioè un piano:
misurato leggendo i vertici, l'altezza cresce lungo +X con correlazione 0,97.
Un pezzo, un piano.

Il muro del kit sta sul bordo **+X** della sua cella: per portarlo sugli altri
lati lo si ruota di 90 gradi alla volta (90 = lato -Z, 180 = -X, 270 = +Z).
`BUILD_CELL` vale 2,6 m, quindi una casa misura 7,8 × 5,2 m con i muri alti
2,6: le stesse dimensioni della scatola procedurale che sostituisce.

Il pezzo del tetto è un segmento a due falde largo **una** cella: affiancarne
due lascia una valle in mezzo, quindi se ne allunga uno solo sulla profondità
(`scale.z = 2`) e si alza il colmo (`scale.y = 1,6`) per non appiattire la
pendenza. La falda è un guscio sottile: da dentro se ne vedrebbe attraverso,
quindi per quei tre pezzi si spegne lo scarto delle facce posteriori — dentro
casa serve un soffitto.

L'ingresso usa `wall-doorway-round`, un arco **aperto**, non `wall-door`, che è
un battente chiuso: da fuori si deve vedere che si può entrare.

### Nelle case si entra

La collisione della casa non è più un cerchio pieno ma i suoi quattro muri, con
il vano della porta lasciato libero (`ResolveHouse()` in `world.c`). Le misure
vengono dalla stessa ricetta che la disegna, così non possono divergere: i muri
cadono a ±1,45 celle in X e ±0,95 in Z, e la porta apre ±0,36 celle al centro
della facciata — 1,87 m contro i 0,90 di raggio del giocatore.

Vale solo quando i pezzi ci sono. Senza modelli la casa resta la scatola
procedurale, e una scatola piena si aggira: la collisione seguirebbe un
disegno che non c'è.

### La camera non guarda mai un muro

Se fra il giocatore e la camera si mette qualcosa, la camera si avvicina finché
il giocatore torna visibile (`WorldCameraClip()` in `world.c`). È la soluzione
abituale in terza persona: rendere trasparente l'edificio richiederebbe di
ordinare le facce per profondità, e da dentro si vedrebbe peggio.

Il taglio è esatto, non campionato — un muro è sottile e fra due campioni ci
passa — e usa due scatole a seconda di dove sta il giocatore:

- **fuori**: la camera non deve *entrare* nell'ingombro dell'edificio, tetto
  compreso. Risolve il caso di chi esce di casa e resta coperto dal muro;
- **dentro**: la camera non deve *uscire* dal vano, soffitto compreso. Risolve
  il caso di chi guarda in basso e si trova la falda davanti all'obiettivo.

Valgono anche torre e cripta (scatole piene) e i **fusti** degli alberi: la
chioma no, attraversare le foglie non dà fastidio e fermarsi a ogni ramo darebbe
una camera nervosa.

La camera rientra all'istante — un fotogramma con il muro davanti si vede — e si
riallontana a `CAM_RETURN_SPEED` metri al secondo: nel bosco fitto, senza,
entrerebbe e uscirebbe a ogni tronco. Misurato camminando in mezzo agli alberi:
un salto brusco ogni 40 campioni invece di uno ogni due.

Quando la camera finisce comunque addosso al collo, il personaggio si
**dissolve** invece di sparire di colpo: da `CAM_FADE_IN` a `CAM_FADE_OUT`
l'opacità va da piena a zero. Il giocatore è disegnato per ultimo fra le cose
solide, quindi la trasparenza non ha bisogno di ordinamenti.

La torre e la cripta restano piene: i pezzi della torre non hanno una porta, e
la cripta è un modello unico.

I pezzi vengono da due kit diversi — Fantasy Town Kit per le case, Castle Kit
per la torre — e ognuno porta il suo `Textures/colormap.png`: stanno quindi in
`assets/models/town/` e `assets/models/castle/`. Se manca anche un solo pezzo
si torna alle primitive per tutti: mezza casa è peggio di una scatola.

### Cosa non ha un equivalente texturizzato

Nei kit CC0 di Kenney non esiste un cespuglio vero: Survival e Mini Forest
hanno solo ciuffi d'erba, e il `plant_bushLarge` del Nature Kit ha la forma
giusta ma è **turchese** (43, 166, 170) e stona sul verde del terreno. Il
cespuglio usa quindi un ciuffo texturizzato ingrandito; l'erba curativa usa il
fiore giallo del Nature Kit, a colori per vertice, perché una quest ha bisogno
che si distingua da un cespuglio.

`LoadExtProps()` in `world.c` li cerca all'avvio; `DrawProp()` usa il modello
quando c'è e ricade sulle primitive quando manca. raylib carica `.glb`/`.gltf`,
`.obj`, `.iqm`, `.m3d` — **non** `.fbx`: in quel caso passare per Blender ed
esportare glTF Binary. Con `.glb` le texture sono dentro al file e vengono
applicate da sole; con `.obj` serve il `.mtl` a fianco.

La tabella `gExtProp` in `world.c` tiene nome del file e fattore di scala:

```c
static const struct { const char *file; float scale; } gExtProp[PROP_COUNT] = {
    [PROP_TREE]  = { "assets/models/tree.glb",  4.61f },
    ...
};
```

La scala converte l'unità del modello nelle dimensioni usate dalle primitive e
si moltiplica per la scala della singola istanza (`p->scale`, fra 0,75 e 1,45
per gli alberi): un albero disegnato sta quindi fra 4,9 e 9,4 m. Per aggiungere
un tipo basta una riga nella tabella — `DrawProp()` usa il modello per
qualunque tipo elencato.

Per i modelli **animati** servono `LoadModelAnimations()` e
`UpdateModelAnimation()` — è il salto di qualità più visibile per gli NPC
(esercizio 10).

### 3. Personaggio animato del giocatore

```bash
./tools/fetch_assets.sh player          # cavaliere
./tools/fetch_assets.sh player Mage     # o Barbarian, Rogue, Rogue_Hooded
```

Scarica un personaggio dal **KayKit Adventurers Character Pack** di Kay
Lousberg (CC0) in `assets/models/player.glb`: un solo file, texture inclusa,
scheletro a 41 ossa e **76 animazioni**. Se il file c'è, in terza persona
sostituisce le primitive.

![Personaggio animato](img/06-personaggio-animato.png)

`src/player.c` non chiama le animazioni per indice ma **le cerca per nome**
(tabella `ANIM_WANTED`): prima per uguaglianza esatta, poi per sottostringa,
ignorando maiuscole. Servono nove ruoli, e all'avvio il gioco stampa la
mappatura trovata:

```
INFO: PLAYER: modello assets/models/player.glb (15 mesh, 41 ossa, 76 animazioni)
INFO: PLAYER:   riposo       -> Idle
INFO: PLAYER:   camminata    -> Walking_A
INFO: PLAYER:   corsa        -> Running_A
INFO: PLAYER:   attacco      -> 1H_Melee_Attack_Slice_Diagonal
INFO: PLAYER:   parata       -> Blocking
INFO: PLAYER:   incantesimo  -> Spellcast_Shoot
INFO: PLAYER:   salto        -> Jump_Idle
INFO: PLAYER:   colpito      -> Hit_A
INFO: PLAYER:   morte        -> Death_A
```

Così funzionano anche altri pacchetti senza toccare il codice: il robot di
Quaternius (`Robot_Walking`, `Robot_Punch`…, CC0, in
`vendor/raylib/examples/models/resources/models/gltf/robot.glb`) e il greenman
degli esempi di raylib (`2_move`, `3_attack`, CC0). Se un ruolo risulta
"assente", si aggiunge il nome giusto ad `ANIM_WANTED`; la scala si regola con
`PLAYER_MODEL_SCALE` in `config.h` (altezza voluta ÷ altezza del modello).

Quale animazione va in scena lo decide `PickAnim()` dallo stato di gioco, in
ordine di priorità: morte, colpo ricevuto, attacco, incantesimo, parata, salto,
corsa, camminata, riposo. Le clip ciclabili scorrono a 60 fps — camminata e
corsa vengono riprodotte in proporzione alla velocità reale, così i piedi non
slittano — mentre quelle a colpo singolo vengono compresse nella durata
dell'azione: il fendente KayKit dura 59 fotogrammi, quasi un secondo, ma il
colpo si esaurisce in 0,4 s, e senza `OneShotSeconds()` la spada si fermerebbe a
metà del movimento.

**Due trappole di raylib, entrambe già gestite.**

1. *I modelli misti fanno crollare l'animazione.* I pacchetti di personaggi
   contengono armi, scudi ed elmi come mesh separate agganciate a un osso.
   `UpdateModelAnimation()` di raylib 5.5 legge però `mesh.boneWeights` su
   **tutte** le mesh del modello, anche su quelle senza pesi, e dereferenzia un
   puntatore nullo: segmentation fault. `BuildSkinnedView()` in `player.c`
   costruisce quindi una vista del modello con le sole mesh dotate di scheletro
   (condividendo i dati, senza copiare nulla) e anima quella.

2. *Le mesh agganciate alle ossa non si muovono.* Sempre perché raylib deforma
   solo le mesh con pesi, spada, scudo, elmo e mantello resterebbero fermi nella
   posa di riposo, sospesi a mezz'aria mentre il corpo si muove — e raylib perde
   anche il nome della mesh e il legame con l'osso. Come vengono agganciati è
   spiegato qui sotto.

#### L'aggancio delle armi

Il problema è di informazioni mancanti, non di matematica: raylib non conserva i
nomi delle mesh né la gerarchia dei nodi, quindi dal solo `Model` non si sa che
la mesh numero 5 è la spada e va attaccata all'osso `handslot.r`.

`tools/glb_attachments.py` ricostruisce la corrispondenza leggendo il glTF e la
scrive accanto al modello, in `assets/models/player.attach`:

```
# Formato: <indice mesh> <nome osso>   # nome originale
5 handslot.r   # arma: 1H_Sword
3 handslot.l   # scudo: Round_Shield
7 head         # elmo: Knight_Helmet
8 chest        # mantello: Knight_Cape

# Alternative: togli il cancelletto per usarle al posto di sopra.
# 6 handslot.r   # 2H_Sword
# 4 handslot.l   # Spike_Shield
```

È un file di testo: per passare allo spadone o a un altro scudo basta spostare
un cancelletto, senza ricompilare. `fetch_assets.sh player` lo genera da sé.

L'indice della mesh si può prevedere perché raylib crea **una mesh per
primitiva visitando i nodi in ordine** (`LoadGLTF` in `rmodels.c`), e ci cuoce
dentro la trasformazione del nodo — cioè i vertici dell'arma sono già in spazio
modello nella posa di riposo. Serve quindi solo portarli dalla posa di riposo a
quella corrente:

```
M = posaCorrente(osso) × inversa(posaDiRiposo(osso))
```

`BoneMatrix()` in `player.c` compone esattamente questa matrice, con la stessa
formula che raylib usa internamente in `UpdateModelAnimationBones()`: usarne una
diversa farebbe muovere l'arma in modo leggermente sfasato rispetto al corpo.
`model.bindPose[osso]` dà la posa di riposo, `anim.framePoses[frame][osso]` quella
corrente, entrambe in spazio modello.

Il controllo che vale la pena fare, se si cambia pacchetto, è guardare
l'animazione di morte: se la matrice è sbagliata il corpo cade e l'arma resta in
piedi a mezz'aria.

### 4. NPC animati

```bash
./tools/fetch_assets.sh npc
```

Installa cinque personaggi CC0 (KayKit *Adventurers* e *Skeletons*), uno per
ruolo:

| File | Chi anima | Modello |
|---|---|---|
| `npc_villager.glb` | popolano, mercante, anziano | Mage |
| `npc_guard.glb` | guardia | Knight |
| `npc_bandit.glb` | bandito | Rogue_Hooded |
| `npc_revenant.glb` | redivivo | Skeleton_Minion |
| `npc_boss.glb` | Vald il Sepolto | Skeleton_Warrior |

![NPC animati](img/07-npc-animati.png)

La tabella `NPC_MODEL` in `entity.c` associa tipo, file e altezza. Tre tipi
puntano allo stesso file, e in quel caso il modello viene caricato una volta
sola. Il **lupo resta procedurale**: fra i pacchetti CC0 usati non c'è un
quadrupede animato.

L'animazione la scegle lo stato dell'IA, che è già una macchina a stati:
`AI_DEAD` → morte, `AI_ATTACK` → attacco, `AI_CHASE` → corsa, `AI_WANDER` →
camminata, altrimenti riposo. Il passo scala con `e->speed`, così un redivivo
lento non pattina come un bandito.

**Un solo modello serve molti personaggi.** `UpdateModelAnimation()` scrive i
vertici deformati nel buffer del modello: contiene una posa alla volta. Non
serve però una copia per NPC — basta rideformare **dentro il ciclo di disegno**,
una volta per personaggio, perché ogni `DrawMesh` usa la posa appena calcolata.
Misurato: **0,06 ms per istanza** (deformazione più caricamento del buffer), cioè
1 ms per sedici NPC visibili su un budget di 16,7 ms a 60 fps. Il numero di
istanze è limitato dal culling, non dalla memoria.

Oltre `NPC_MODEL_DIST` (140 m) si torna alle primitive: a quella distanza un
passo non si distingue e la rideformazione non ripaga.

**Il costo vero è la memoria.** Ogni pacchetto porta 76-95 clip di cui il gioco
usa nove, e una posa sono 41 ossa × 40 byte per ogni fotogramma di ogni clip.
Misurato con `/usr/bin/time` (picco di RSS):

| Configurazione | Picco RSS |
|---|---|
| senza modelli di personaggio | 154 MB |
| sei modelli, tutte le clip | 245 MB |
| sei modelli, clip inutilizzate liberate | 207 MB |

`DropUnusedAnims()` in `charmodel.c` libera le pose delle clip che nessun ruolo
usa, azzerandone `frameCount` perché `UnloadModelAnimations()` non le ripassi.
Restano ~53 MB per sei personaggi: sul desktop non è un problema, ma la build
WebAssembly gira con `TOTAL_MEMORY=134217728` e va alzata. Il passo successivo,
se serve, è togliere le clip inutilizzate direttamente dal `.glb`.

### 5. Font dell'interfaccia

```bash
./tools/fetch_assets.sh font
```

Scarica due font **OFL** dal repository di Google Fonts, verificando la licenza
prima di usarli: `ui.ttf` (Alegreya Sans) per il testo e `title.ttf` (Cinzel)
per i titoli. Se mancano si usa il font di raylib, che è una bitmap e su uno
schermo grande si sgrana.

Dentro `ui.c` le chiamate sono rimaste quelle di prima nella forma: `UiText()`
ha la stessa firma di `DrawText()` e `UiTextWidth()` quella di `MeasureText()`.
Cambia solo chi disegna — così il resto del file non si è dovuto riscrivere.

Due dettagli che si vedono se si sbagliano:

- i font si **caricano grandi** (64 px) e si rimpiccioliscono, con filtro
  bilineare: il contrario dà bordi impastati;
- il font da titolo scatta da solo oltre i 26 punti, invece di essere scelto a
  mano in ogni chiamata.

### 6. Audio

Frostmark non usa audio: aggiungerlo richiede solo `InitAudioDevice()` in
`main.c` e quattro chiamate. Suoni consigliati: colpo a segno, passo, ambiente,
musica di sottofondo del villaggio.

```c
InitAudioDevice();
Sound hit = LoadSound("assets/audio/hit.wav");
Music amb = LoadMusicStream("assets/audio/ambient_wind.ogg");
PlayMusicStream(amb);
/* nel loop */ UpdateMusicStream(amb);
/* quando il colpo va a segno */ PlaySound(hit);
```

Il volume dell'ambiente può essere modulato dal bioma e dall'ora del giorno: due
righe, e il mondo cambia carattere.

---

## Come tenere pulita la licenza

1. Un file `assets/CREDITS.md` con: nome asset, autore, fonte, licenza, data di
   download. Anche quando CC0 non lo richiede.
2. Non committare asset di terze parti nel repository se non si è certi della
   licenza: meglio uno script di download.
3. Se si accettano asset CC-BY, l'attribuzione deve comparire **nel gioco**
   (schermata crediti), non solo nel README.
