# 02 — Generazione del mondo

## Il rumore

`noise.c` implementa un **value noise** su hash intero. Non usa tabelle di
permutazione né numeri casuali di sistema: il valore di ogni nodo della griglia
è `hash(x, z, seed)`. Questo garantisce che lo stesso seme produca lo stesso
mondo su qualunque macchina e in qualunque ordine vengano fatte le richieste
(fondamentale, perché i chunk vengono generati in ordine imprevedibile).

Tre livelli:

| Funzione | Uso |
|---|---|
| `Noise2D` | un'ottava, interpolazione con la curva di Perlin `6t⁵−15t⁴+10t³` |
| `NoiseFBM` | somma di ottave: forme organiche, continenti, umidità |
| `NoiseRidged` | `1 − |2n − 1|`, poi al quadrato: creste montuose |

## La formula del terreno

In `RawHeight()`:

```
continenti = fBm(0.00085)^1.55        forma le masse di terra
montagne   = ridged(0.0026)           catene affilate
maschera   = smoothstep(0.42, 0.78, continenti)
colline    = fBm(0.006)
dettaglio  = fBm(0.035)

h = continenti·62 + maschera·montagne·90 + colline·10 + (dettaglio−0.5)·3.5 − 8
```

Poi una fascia di *smoothstep* attorno al livello del mare appiattisce le coste,
così le spiagge non sono pareti verticali.

**Perché la maschera.** Moltiplicare il rumore delle montagne per una funzione
delle quote basse impedisce che spuntino picchi in mezzo all'oceano: le catene
compaiono solo dove il continente è già alto. È lo stesso trucco usato dai
generatori "a domini annidati".

### Spianare i villaggi

Un villaggio su un pendio è ingiocabile. `WorldHeight()` interpola l'altezza
grezza verso la quota del centro abitato:

```c
float t = smoothstep(radius*0.45, radius, distanza_dal_centro);
h = lerp(town.baseHeight, h, t);
```

Attenzione alla ricorsione: `baseHeight` viene calcolato con `RawHeight()`
(senza spianamento), altrimenti la funzione chiamerebbe sé stessa.

Questa tecnica — modificare l'altezza in funzione di elementi "logici" del mondo
— si estende naturalmente a strade, fiumi, crateri e radure.

## Biomi

`WorldBiomeAt()` combina quota e un secondo campo di rumore ("umidità"):

```
h < 14                 → oceano
h < 16.2               → spiaggia
h > 78                 → nevi
h > 55                 → montagna
umidità > 0.52, h < 46 → foresta
h > 34                 → colline
altrimenti             → pianura
```

Il bioma determina il colore dei vertici, la densità e il tipo di vegetazione e
il tipo di nemici che compaiono. È il punto più semplice da estendere: aggiungere
paludi, tundra o deserti richiede un valore nell'enum, un colore e una riga nello
scatter.

## Distribuzione dei prop

Alberi, rocce ed erbe non usano numeri casuali: ogni chunk è diviso in 10 × 10
celle e in ogni cella l'oggetto è posizionato con un *jitter* derivato da
`NoiseHash01(seed, id_cella, k)`. È un Poisson disk "dei poveri": evita
sovrapposizioni evidenti restando completamente deterministico e senza stato.

Vincoli applicati: niente vegetazione sott'acqua, niente su pendenze con
`normal.y < 0.72` (lì compaiono rocce), niente dentro il raggio dei villaggi.

---

## Usare tool open source al posto del rumore

Il gioco carica automaticamente `assets/heightmap.png` se presente: un PNG in
scala di grigi viene campionato bilinearmente e sostituisce completamente la
formula procedurale (la scala verticale è 110 m, si cambia in `RawHeight()`).

```c
if (FileExists("assets/heightmap.png")) { ... w->useHeightmap = true; }
```

Ecco tre pipeline realistiche, tutte con strumenti liberi.

### A. Dati reali da satellite (QGIS + GDAL)

1. Scaricare un tile DEM di pubblico dominio: **SRTM** o **ASTER GDEM** da
   [earthexplorer.usgs.gov](https://earthexplorer.usgs.gov/), oppure **Copernicus
   DEM** dal portale ESA.
2. In QGIS: caricare il GeoTIFF, ritagliare l'area (*Raster → Extraction → Clip*).
3. Esportare in PNG a 8 bit:

```bash
gdal_translate -of PNG -ot Byte -scale -outsize 1024 1024 dem.tif heightmap.png
```

`-scale` normalizza il range altimetrico su 0–255. Il risultato va in `assets/`.

### B. Terreno modellato a mano (Blender + A.N.T. Landscape)

1. Abilitare l'add-on **A.N.T. Landscape** (incluso in Blender, GPL).
2. Generare o scolpire il rilievo, poi *bake* dell'altezza su un'immagine, oppure
   render ortografico dall'alto con materiale *Emission* pilotato da
   `Geometry → Position → Z` normalizzato.
3. Salvare come PNG in scala di grigi.

### C. Disegno diretto (GIMP / Krita)

Un livello grigio, pennello morbido, *Filters → Render → Clouds → Solid Noise*
per il dettaglio, sfocatura gaussiana per addolcire. Perfetto per progettare la
mappa di un livello a mano.

### Cosa cambia con una heightmap

Con `useHeightmap = true` il mondo smette di essere infinitamente riproducibile
da un seme: il salvataggio continua a funzionare, ma il file PNG diventa parte
dei dati di gioco. I villaggi vengono comunque posizionati con la ricerca
automatica di terreno pianeggiante, quindi la mappa resta giocabile.

### Altri strumenti utili

| Strumento | Licenza | A cosa serve |
|---|---|---|
| **libnoise** / **FastNoiseLite** | LGPL / MIT | rumore più ricco (simplex, cellular, domain warp) |
| **QGIS + GDAL** | GPL / MIT | dati altimetrici reali |
| **Blender + A.N.T.** | GPL | terreni scolpiti, erosione idraulica |
| **Tiled** | GPL/BSD | posizionare a mano villaggi e punti d'interesse su una griglia |
| **Godot HTerrain / valentinegb erosion** | MIT | riferimenti per algoritmi di erosione |

Il candidato più naturale per un'evoluzione è **FastNoiseLite**: è un singolo
header C, quindi non tradisce il vincolo "solo librerie strettamente necessarie",
e sostituirebbe `noise.c` mantenendo la stessa interfaccia.
