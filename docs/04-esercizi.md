# 04 — Esercizi

Dodici esercizi in ordine di difficoltà crescente. Ognuno indica i file da
toccare e un suggerimento sul punto delicato. Sono pensati per essere fatti in
ordine, ma i primi cinque sono indipendenti fra loro.

---

## Livello 1 — Prendere confidenza

### 1. Cambiare la scala del mondo
**File**: `config.h`

Portare `WORLD_CHUNKS` a 128 e `VIEW_CHUNKS` a 7. Misurare gli FPS prima e dopo.
Poi provare `CHUNK_QUADS` a 16 e a 64: cosa cambia nella silhouette delle
montagne e nel tempo di costruzione di un chunk?

*Punto delicato*: con `CHUNK_QUADS` sopra 255 il numero di vertici supera i
65.535 indicizzabili con `unsigned short` — la mesh si corrompe. Serve passare a
`unsigned int` e a `UploadMesh` con indici a 32 bit.

### 2. Aggiungere un nuovo bioma
**File**: `worldtypes.h` (`Biome`), `tools/worldgen.c` (`GenBiomeAtHeight`,
`GenChunkProps`), `world.c` (`WorldBiomeColor`, `WorldBiomeName`)

Aggiungere `BIOME_SWAMP`: quota bassa, umidità molto alta, colore verde-bruno,
vegetazione fitta di cespugli e nessun pino.

*Punto delicato*: il bioma è un dato cotto, quindi va ricotto il mondo
(`make mondo-forza`) — e il colore, che invece è codice, sta nel gioco. È la
divisione da capire prima di toccare qualsiasi altra cosa nel mondo.

### 3. Un nuovo oggetto e una nuova arma
**File**: `items.c/h`

Aggiungere un arco (`IK_WEAPON` con `power` basso) e usarlo come condizione per
un attacco a distanza fisico invece del dardo di fuoco.

### 4. Una quarta missione
**File**: `quest.c` (tabella `QUESTS`, `QuestForNPC`, `DialogueBuild`)

"Consegna un pacco al villaggio vicino": la quest si completa quando il giocatore
entra nel raggio di un altro villaggio. Serve un controllo in `UpdatePlaying()`
simile a `UpdateCrypt()`.

---

## Livello 2 — Sistemi

### 5. Bottino raccoglibile invece che automatico
**File**: `game.c` (`GiveLoot`, `DoInteract`), `entity.h`

Oggi il bottino finisce nello zaino automaticamente. Trasformare i cadaveri in
contenitori: aggiungere un array `InvSlot loot[4]` all'entità e una schermata
`GS_LOOT` che permetta di svuotarli con `E`.

### 6. Fame, sonno e riposo
**File**: `player.c/h`, `ui.c`

Aggiungere una barra di stanchezza che cresce nel tempo e riduce la rigenerazione
di vigore. Dormire in un letto (nuovo `PROP_BED` dentro le case) avanza
`timeOfDay` fino al mattino e ripristina le statistiche.

*Punto delicato*: far avanzare il tempo di gioco a scatti significa saltare molti
frame di `EntitiesUpdate` — decidere cosa deve succedere ai nemici nel frattempo.

### 7. Salvataggio delle entità
**File**: `save.c`

Oggi il salvataggio contiene l'identità del mondo e il giocatore, e i nemici
respawnano.
Estendere `SaveData` con l'array delle entità persistenti (boss e NPC di
villaggio), incrementando `SAVE_VERSION` e gestendo il caricamento delle versioni
precedenti.

*Punto delicato*: `SaveData` viene scritto con `fwrite` di una struct. È veloce
ma dipende da padding ed endianness. Un formato testuale chiave=valore è più
lungo da scrivere ma portabile e ispezionabile: valutare il compromesso.

---

## Livello 3 — Grafica

### 8. Texture splatting
**File**: `world.c` (`BuildChunkMesh`), shader nuovo

Invece di un'unica texture grigia moltiplicata per il colore del vertice, mescolare
erba/roccia/neve in base a quota e pendenza. Serve uno shader frammento con tre
sampler e i pesi passati nei colori dei vertici (canali R, G, B) o in
`texcoords2`.

### 9. Nebbia distanziale e vero illuminamento
**File**: shader nuovo, `game.c`

Scrivere un vertex+fragment shader con Phong a una luce direzionale e nebbia
esponenziale sul colore del cielo. Caricarlo con `LoadShader()` e assegnarlo a
`w->terrainMat.shader` e ai materiali dei modelli.

*Punto delicato*: raylib usa GLSL 330 su desktop e GLSL 100 su web/mobile.
Servono due file, selezionati con `#if defined(PLATFORM_WEB)`, come negli esempi
ufficiali di raylib.

### 10. Modelli animati per gli NPC
**File**: `entity.c` (`EntitiesDraw`), `world.c` (caricamento)

Sostituire capsule e sfere con modelli glTF animati (Quaternius, CC0), usando
`LoadModelAnimations()` e `UpdateModelAnimation()`. Scegliere l'animazione in
base a `AIState` (idle / cammina / attacca / morte).

*Punto delicato*: `UpdateModelAnimation` modifica la mesh del modello, quindi
tutte le entità che condividono lo stesso `Model` condividono anche il frame di
animazione. Serve un modello per entità, oppure animazione via skinning nello
shader.

---

## Livello 4 — Impegnativi

### 11. Frustum culling vero e livelli di dettaglio
**File**: `world.c` (`InView`, `WorldDrawTerrain`, `WorldDrawProps`)

Oggi il culling è un semplice prodotto scalare sul vettore forward: scarta troppo
poco ai lati e nulla in verticale. Estrarre i sei piani del frustum dalla matrice
vista-proiezione e testare la bounding box di ogni chunk. Poi generare mesh a
risoluzione dimezzata per i chunk oltre 3 anelli di distanza.

*Punto delicato*: le mesh a risoluzione diversa creano fessure ai bordi
(*T-junction*). La soluzione classica è "cucire" il bordo del chunk a risoluzione
alta abbassando i vertici dispari, oppure usare un bordo verticale ("skirt")
nascosto sotto il terreno.

### 12. Interni e dungeon
**File**: nuovo modulo `dungeon.c/h`, `game.c`

Trasformare la cripta in un vero interno: entrare da una porta cambia stato
(`GS_DUNGEON`), sostituisce il mondo con una mappa a celle generata con un
algoritmo di stanze-e-corridoi, e conserva la posizione all'esterno per l'uscita.

*Punto delicato*: il gioco assume ovunque che il terreno sia una funzione di
`(x, z)`. Un interno con più piani rompe questa assunzione. Il modo più pulito è
introdurre un'interfaccia comune (`float GroundAt(x, z)`) e due implementazioni,
piuttosto che infilare condizioni ovunque.

---

## Suggerimento generale

Prima di ogni esercizio, compilare con `make debug` e tenere aperta la finestra
del terminale: `TraceLog(LOG_INFO, ...)` è lo strumento di indagine più efficace
del progetto. E dopo ogni modifica alla generazione del mondo, ricuocere con lo
**stesso seme** (`make mondo-forza`) e controllare con `make verifica-mondo`: se
il confronto col generatore non è a zero, il mondo cotto e il codice non
raccontano più la stessa cosa.
