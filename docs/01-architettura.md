# 01 — Architettura

## Il principio guida

Un gioco di questo tipo può facilmente diventare un groviglio. Frostmark segue
tre regole, e tutto il resto discende da lì.

**Regola 1 — Il mondo si interroga da un solo posto.**
`WorldHeight(world, x, z)` restituisce l'altezza del terreno in qualunque punto.
Chi la chiama non sa da dove viene, e questo è il punto: collisioni, mesh,
minimappa e villaggi passano tutti da lì.

- la collisione con il terreno è una riga: `if (pos.y <= WorldHeight(...))`;
- la mesh di un chunk si costruisce campionando la stessa funzione;
- la mappa del mondo si disegna campionando la stessa funzione su una griglia;
- non esistono desincronizzazioni tra "quello che vedo" e "dove sbatto".

Fino alla fase 3 del piano (`docs/05`) quella funzione *calcolava* l'altezza dal
seme: il mondo era una funzione, non un dato. Ora legge una griglia cotta una
volta da `tools/baker` e caricata da `assets/world/` — 2048 × 2048 quote a 2 m di
passo, 8,4 MB. La firma è la stessa, i chiamanti non sono cambiati, ma il mondo è
diventato **autoriale**: si può spostare un albero o spianare una radura, e la
modifica resta. Era il presupposto della fisica.

Il prezzo pagato, dichiarato: il salvataggio non è più un `unsigned int` che
rigenera tutto — il seme che contiene serve solo a riconoscere il mondo e a
rifiutare una partita fatta in un altro — e senza `assets/world/` il gioco non
parte.

**Regola 2 — Lo stato del gioco sta in una sola struttura.**
`Game` (in `game.h`) contiene mondo, giocatore, entità, proiettili, quest e stato
dell'interfaccia. Viene allocata `static` in `main.c` e passata per puntatore.
Niente variabili globali sparse, niente singleton, niente allocazioni dinamiche
nel ciclo di gioco.

**Regola 3 — L'interfaccia è una macchina a stati.**
`GameState` (`GS_MENU`, `GS_PLAY`, `GS_INVENTORY`, …) decide sia quale funzione
di aggiornamento gira, sia quale schermata viene disegnata. Aggiungere una
schermata significa: un valore nell'enum, un `case` in `GameUpdate`, un `case` in
`GameDraw`, una funzione in `ui.c`. Nient'altro.

## Il ciclo di gioco

```
main()
 └─ while (!WindowShouldClose())
     ├─ GameInput(g)                    una volta per fotogramma
     │   └─ switch (g->state)            tasti, mouse, menu, dialoghi
     ├─ GameSimulate(g, SIM_STEP)        a passo fisso, anche piu' volte
     │   └─ GS_PLAY → UpdatePlaying()
     │            ├─ PlayerUpdate()          movimento, gravità, stamina
     │            ├─ WorldUpdateStreaming()   carica/scarica chunk
     │            ├─ EntitiesPopulate()       spawn/despawn nemici
     │            ├─ EntitiesUpdate()         IA e attacchi
     │            ├─ ProjUpdate()             proiettili
     │            └─ input di combattimento/interazione
     ├─ GameUpdateCamera(g)              dopo i passi, una volta per fotogramma
     │                                    (dipende da yaw e pitch, che l'input
     │                                     aggiorna per fotogramma)
     └─ GameDraw(g)
         ├─ BeginMode3D → terreno, prop, entità, proiettili, acqua
         └─ 2D → marker, HUD, schermata attiva

`PlayerUpdate()` in discesa si **aggancia al terreno**: senza, a ogni passo il
suolo scende sotto i piedi, il giocatore resta in aria per un fotogramma e la
gravità lo riprende — un sobbalzo ritmico di pochi centimetri, e `onGround`
falso quasi sempre, quindi niente salto e niente parata. Ci si incolla solo per
il dislivello che un passo può giustificare (`STEP_DOWN_SLOPE` in `config.h`):
un salto nel vuoto resta una caduta, con il suo danno.

Il giocatore cammina sulla **superficie che vede**: `WorldIoHeight()` legge la
quota sui due triangoli del quadrato, con lo stesso taglio che usa
`BuildChunkMesh()`, non su una superficie bilineare. Le due differiscono di
pochi centimetri, ma la differenza cambia a ogni quadrato attraversato:
misurata correndo in diagonale a 9 m/s, 2,3 cm di ampiezza a quasi quattro
oscillazioni al secondo — un saltellio fine che si sentiva proprio correndo.
Lungo un asse della griglia le due superfici coincidono, ed è per questo che il
difetto spariva andando dritti a nord o a est.

### Luce e ombre

Prima non c'era luce: il terreno portava un'illuminazione **cotta** nei colori
dei vertici, calcolata una volta da una direzione fissa, e tutto il resto veniva
solo moltiplicato per la tinta del ciclo giorno/notte. Nessuna faccia era più
chiara di un'altra e niente proiettava ombra.

Ora `src/light.c` tiene un sole direzionale e una mappa di profondità vista dal
sole. Lo shader (`assets/shaders/scene.vs|fs`) è uno solo e serve terreno, prop
e personaggi: nel passaggio d'ombra scrive soltanto la profondità, nel disegno
vero somma ambiente e sole e legge l'ombra dalla mappa.

Tre decisioni che si vedono:

- **Le mappe sono due**, entrambe 2048 texel: una stretta attorno al giocatore
  (48 m di lato, 2,3 cm per texel) e una larga per il resto (120 m, 5,9 cm). Con
  una sola bisognava scegliere fra ombre nitide e ombre lontane: a 3,9 cm per
  texel, a tre metri dalla camera un texel copriva quasi sette pixel di schermo
  e i blocchi si vedevano. Le due passate costano 3,3 ms.
- **Il quadrato della mappa è spostato verso il sole**, non centrato sul
  giocatore: le ombre cadono dalla parte opposta al sole, quindi chi può
  oscurarti sta dalla parte del sole. Centrandolo sul giocatore, una torre a
  venti metri restava fuori e la sua ombra spariva.
- **La proiezione dev'essere quadrata quanto la mappa.** Accendendo il
  framebuffer a mano, `BeginMode3D()` prendeva l'aspetto dello *schermo*: la
  proiezione copriva 124 m in orizzontale contro 70 in verticale, schiacciati
  negli stessi texel, e le ombre uscivano a scaletta. `BeginTextureMode()` dice
  a raylib quanto è grande il bersaglio, e l'aspetto torna 1:1.
- **Il sole pesa quasi il doppio dell'ambiente** (0,85 contro 0,45). Con i due
  alla pari l'ombra toglieva un quarto della luce e non si vedeva.
- **La luce cotta nel terreno sparisce** quando lo shader c'è, altrimenti le
  colline sarebbero scure due volte. Senza `assets/shaders/` il gioco torna
  esattamente a com'era: l'assenza di un file non è un errore.

Giocatore ed entità non si attraversano: `EntitiesPushPlayer()` in `entity.c`
li separa come due cerchi sul piano, dopo che tutti si sono mossi — sta lì e
non in `PlayerUpdate()` perché il giocatore non conosce le entità. Lo
spostamento non si divide in parti uguali: la parte grossa la prende l'entità,
così camminando addosso a un popolano lo si scansa mentre un lupo che carica
non ti sposta di peso. Sui morti si cammina.

Le entità seguono la stessa fisica (`EntityFall()` in `entity.c`), che gira una
volta per entità a fine aggiornamento, qualunque cosa abbia deciso l'IA: prima
stava dentro il movimento, e un nemico fermo lasciato a mezz'aria non cadeva
mai. Non prendono danno da caduta: un lupo che si butta da una rupe darebbe
esperienza e bottino senza che nessuno lo abbia ucciso.
```

Nota su `dt`: viene limitato a 0,05 s in `main.c`. Senza questo limite, dopo una
pausa del sistema operativo il giocatore attraverserebbe il terreno in un frame.

## Streaming dei chunk

Il mondo è una griglia di 64 × 64 chunk da 64 m. Ne restano caricati al massimo
`(2·5+1)² = 121`, cioè un quadrato di 11 × 11 chunk attorno al giocatore.

`WorldUpdateStreaming()` fa due cose:

1. scarica i chunk oltre `VIEW_CHUNKS + 1` (isteresi: evita il caricamento
   ciclico quando si cammina lungo un confine);
2. carica i mancanti **al massimo 3 per frame** (`CHUNK_BUILDS_PER_FRAME`),
   procedendo per anelli concentrici dal giocatore verso l'esterno.

Costruire una mesh richiede 33 × 33 = 1.089 letture di `WorldHeight` più
altrettante di `WorldNormalAt` (che ne fa altre 4). Erano 5.445 valutazioni di
rumore, la parte più costosa del gioco; da quando il mondo è cotto sono 5.445
interpolazioni bilineari su una griglia in memoria. Restano distribuite su più
frame perché il caricamento sulla GPU non è gratis, non più perché il calcolo lo
sia.

I prop non si spargono più a ogni caricamento di chunk: si leggono da
`props.bin`, dove sono indicizzati per chunk, dieci byte per istanza.

## Le due visuali

`PlayerCamera()` costruisce la camera in prima o in terza persona partendo dagli
stessi dati: posizione, `yaw`, `pitch`. In soggettiva la camera sta negli occhi
(`PLAYER_EYE`) con un po' di *head bob*; in terza persona arretra di `camDist`
lungo la direzione della visuale, con uno scostamento in alto e a destra
(`CAM_RISE`, `CAM_SHOULDER`) perché altrimenti il personaggio coprirebbe
esattamente il punto inquadrato.

Due dettagli che è facile sbagliare:

**La mira non passa dalla camera.** Prima, mischia, magia e dialoghi usavano
`cam.target - cam.position`. In terza persona quel vettore parte da tre metri
dietro le spalle: il dardo di fuoco sarebbe nato dietro al giocatore e i
bersagli sarebbero risultati più lontani del vero. Ora esistono `PlayerEye()` e
`PlayerLookDir()`, e `EntityLookedAt()` prende origine e direzione invece di una
`Camera3D`. Il combattimento è quindi identico nelle due visuali, e la camera
resta un puro fatto di presentazione.

**Il terreno si mette in mezzo.** Se la camera arretrasse in linea retta,
salendo una collina finirebbe sotto la superficie. `PlayerCamera()` campiona
`WorldHeight()` in otto punti lungo l'arretramento e accorcia la distanza al
primo che sfonda, poi alza comunque la camera di `CAM_CLEARANCE` sul terreno.
Costa una manciata di `WorldHeight()` per frame, cioè niente: sono letture di una
griglia in memoria. I prop non sono considerati: un albero fra camera e
giocatore lo si attraversa, come in molti giochi veri.

`charmodel.c` contiene il modello di personaggio animato, condiviso fra
giocatore e NPC: ricerca delle clip per nome, vista con le sole mesh skinnate
(altrimenti `UpdateModelAnimation()` di raylib crolla) e agganci di arma e scudo
alle ossa. La **posa non sta nel modello** ma in chi lo disegna, e per questo lo
stesso modello serve molti personaggi con animazioni diverse: si rideforma una
volta per personaggio dentro il ciclo di disegno.

Il corpo del giocatore (`PlayerDraw()`) è disegnato solo in terza persona. Se
`assets/models/player.glb` esiste si usa quel modello con le sue animazioni
(vedi `docs/03-asset-pubblici.md`), altrimenti le stesse primitive dei bipedi di
`entity.c`: capsula, testa, arma, più due gambe che oscillano su `bobPhase` — la
stessa fase che in soggettiva muove la testa.

![Terza persona](img/05-terza-persona.png)

## Illuminazione senza shader

Lo shader di default di raylib non calcola illuminazione. Invece di scriverne uno
(che introdurrebbe differenze GLSL 100/330 e problemi di portabilità), la luce
diffusa viene **pre-calcolata nei colori dei vertici** quando la mesh viene
costruita:

```c
float diff = Vector3DotProduct(normal, SUN_DIR);
float lit  = 0.42f + 0.58f * fmaxf(diff, 0.0f);
```

Il ciclo giorno/notte moltiplica poi l'intero fotogramma per un colore ambiente
(`GameAmbientTint`) applicato come `colDiffuse` del materiale e come tinta dei
modelli. È un compromesso: il sole non proietta ombre dinamiche, ma il costo è
zero e il codice resta a portata di chi sta imparando.
`docs/04-esercizi.md` propone come sostituirlo con un vero shader di Phong.

## Costi e limiti noti

| Aspetto | Scelta attuale | Limite |
|---|---|---|
| Disegno terreno | 1 `DrawMesh` per chunk | ~121 draw call |
| Prop | 1–5 `DrawModelEx` ciascuno | pesante nelle foreste dense |
| Personaggi animati | skinning su CPU, ~0,06 ms per istanza | 6 draw call ciascuno; oltre 140 m tornano primitive |
| Culling | solo dot product sul forward | niente frustum vero |
| Collisioni | cerchi 2D contro i prop | niente collisione verticale |
| Nemici | 12 attivi attorno al giocatore | nessuna persistenza |

Sono tutti punti volutamente semplici: ognuno è un esercizio nel documento 04.
