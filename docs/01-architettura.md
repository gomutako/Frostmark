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
     ├─ GameUpdate(g, dt)
     │   └─ switch (g->state)
     │       └─ GS_PLAY → UpdatePlaying()
     │            ├─ PlayerUpdate()          movimento, gravità, stamina
     │            ├─ PlayerCamera()           camera dalla posizione+yaw/pitch
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
| Culling | solo dot product sul forward | niente frustum vero |
| Collisioni | cerchi 2D contro i prop | niente collisione verticale |
| Nemici | 12 attivi attorno al giocatore | nessuna persistenza |

Sono tutti punti volutamente semplici: ognuno è un esercizio nel documento 04.
