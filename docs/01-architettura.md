# 01 — Architettura

## Il principio guida

Un gioco di questo tipo può facilmente diventare un groviglio. Frostmark segue
tre regole, e tutto il resto discende da lì.

**Regola 1 — Il mondo è una funzione, non un dato.**
`WorldHeight(world, x, z)` restituisce l'altezza del terreno in qualunque punto,
senza consultare alcuna struttura dati. Conseguenze pratiche:

- la collisione con il terreno è una riga: `if (pos.y <= WorldHeight(...))`;
- la mesh di un chunk si costruisce campionando la stessa funzione;
- la mappa del mondo si disegna campionando la stessa funzione su una griglia;
- il salvataggio contiene un `unsigned int` invece di 16 MB di altimetria;
- non esistono desincronizzazioni tra "quello che vedo" e "dove sbatto".

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
     │            ├─ PlayerCamera()           camera in prima o terza persona
     │            ├─ WorldUpdateStreaming()   carica/scarica chunk
     │            ├─ EntitiesPopulate()       spawn/despawn nemici
     │            ├─ EntitiesUpdate()         IA e attacchi
     │            ├─ ProjUpdate()             proiettili
     │            └─ input di combattimento/interazione
     └─ GameDraw(g)
         ├─ BeginMode3D → terreno, prop, entità, proiettili, acqua
         └─ 2D → marker, HUD, schermata attiva
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

Costruire una mesh richiede 33 × 33 = 1.089 campionamenti di `WorldHeight` più
altrettanti di `WorldNormalAt` (che ne fa altri 4): è la parte più costosa del
gioco, e per questo è distribuita su più frame.

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
Costa una manciata di `WorldHeight()` per frame, cioè niente, perché l'altezza è
una funzione pura. I prop non sono considerati: un albero fra camera e
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
