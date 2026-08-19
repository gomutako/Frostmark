# Frostmark

Un piccolo RPG open world in prima persona ispirato a *The Elder Scrolls V: Skyrim*,
scritto in **C99 standard** con la sola libreria **raylib**.

Il progetto ha due obiettivi dichiarati:

1. **Didattico** — ogni sistema (mondo, IA, quest, inventario, salvataggio) sta in
   un modulo breve e leggibile, senza astrazioni inutili. ~2.700 righe in totale.
2. **Operativo** — non è uno scheletro: si compila, si gioca, si finisce.

Il mondo è **generato proceduralmente** (4096 × 4096 metri), non ci sono asset
binari obbligatori: texture, terreno, edifici e personaggi sono costruiti a
runtime. Gli asset esterni sono **opzionali** e, quando servono, si usano solo
fonti pubbliche CC0 (vedi `docs/03-asset-pubblici.md`).

![Foresta](docs/img/01-foresta.png)
![Villaggio](docs/img/02-villaggio.png)

---

## Compilazione

Serve un compilatore C99 e raylib (≥ 4.5, testato con 5.5). Il Makefile cerca
raylib in quest'ordine: `RAYLIB_PATH=dir` → submodule `vendor/raylib` →
`pkg-config` → `-lraylib`.

### Con il submodule (consigliato: versione fissata, niente pacchetti di sistema)

raylib 5.5 è agganciata come submodule in `vendor/raylib`.

```bash
git clone --recurse-submodules git@github.com:gomutako/Frostmark.git
cd Frostmark
make raylib          # compila libraylib.a dai sorgenti, una volta sola (~1 min)
make && ./frostmark
```

Se il repository è già clonato senza submodule:

```bash
git submodule update --init vendor/raylib
```

Su Linux la compilazione di raylib richiede gli header di sviluppo di X11 e
OpenGL:

```bash
sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libxi-dev libgl1-mesa-dev
```

`make raylib-clean` ributta via la libreria compilata; `make clean` non la
tocca.

### Con raylib installata nel sistema

Se il submodule non è inizializzato, il Makefile ricade su `pkg-config`.

```bash
sudo apt install build-essential libraylib-dev   # Debian/Ubuntu, se esiste
brew install raylib                              # macOS
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-raylib make   # MSYS2/MinGW-w64
make
```

### Con una release precompilata

```bash
curl -LO https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_linux_amd64.tar.gz
mkdir -p vendor && tar xzf raylib-5.5_linux_amd64.tar.gz -C vendor/
make RAYLIB_PATH=vendor/raylib-5.5_linux_amd64
```

### Browser (WebAssembly)

```bash
make raylib-web      # compila raylib per il browser dal submodule
make web             # richiede emsdk attivo
```

### Asset opzionali

```bash
./tools/fetch_assets.sh              # prepara assets/ e stampa le fonti CC0
./tools/fetch_assets.sh heightmap    # genera anche una heightmap di prova
```

Gli asset riconosciuti in automatico, senza toccare il codice — se mancano, il
gioco usa la versione procedurale:

| File | Effetto |
|---|---|
| `assets/heightmap.png` | terreno esterno al posto del rumore (QGIS/GDAL, Blender, GIMP — vedi `docs/02-generazione-mondo.md`) |
| `assets/textures/grass.png` | texture del terreno al posto della grana procedurale |
| `assets/models/tree.glb`, `pine.glb`, `rock.glb`, `house.glb` | modelli al posto delle primitive |

Font e audio richiedono invece qualche riga di codice: vedi
`docs/03-asset-pubblici.md`.

![Mondo da heightmap esterna](docs/img/03-heightmap-esterna.png)

### Avvio con un seme specifico

```bash
./frostmark            # mondo predefinito
./frostmark 12345      # mondo generato dal seme 12345
```

---

## Comandi

| Tasto | Azione |
|---|---|
| `W A S D` | Movimento |
| `Mouse` | Guardarsi attorno |
| `Shift` | Corsa (consuma vigore) |
| `Spazio` | Salto |
| `Tasto sinistro` | Attacco in mischia |
| `Tasto destro` | Dardo di fuoco (consuma magia) |
| `E` | Parla / raccogli |
| `R` | Bevi rapidamente una pozione di cura |
| `TAB` | Zaino (inventario) |
| `J` | Diario delle missioni |
| `M` | Mappa del mondo |
| `ESC` | Pausa (`F5` salva, `F9` carica) |

Nei menu: `↑ ↓` per scorrere, `Invio` per confermare, `ESC` per uscire.

---

## Cosa c'è nel gioco

- **Mondo aperto** 4096 × 4096 m con biomi (oceano, spiaggia, pianura, foresta,
  colline, montagna, nevi), streaming dei chunk e distanza visiva di ~320 m.
- **5 villaggi** generati proceduralmente con case, torre, popolani, guardie,
  un mercante e un anziano; il terreno viene spianato automaticamente sotto
  l'abitato.
- **Ciclo giorno/notte** (12 minuti reali = 24 ore di gioco) che tinta cielo,
  terreno e personaggi.
- **Combattimento** in mischia a cono + magia a proiettile, con vigore e magia.
- **Nemici** con IA a stati (lupi, banditi, redivivi) che compaiono in base al
  bioma, e un **boss** nella cripta.
- **3 missioni** concatenate: caccia ai lupi → erbe curative → il Sepolto.
- **Inventario, equipaggiamento, negozio, dialoghi, livelli ed esperienza.**
- **Salvataggio** (solo seme + stato del giocatore: il mondo si rigenera identico).
- **Mappa e minimappa** disegnate campionando la stessa funzione di altezza.

---

## Struttura del codice

```
src/
  config.h    tutte le costanti di gioco (scala del mondo, bilanciamento)
  noise.c/h   value noise, fBm, ridged noise deterministici
  world.c/h   altimetria, biomi, mesh dei chunk, prop, villaggi, collisioni
  player.c/h  movimento in prima persona, statistiche, camera
  entity.c/h  NPC, nemici, IA a stati finiti, proiettili
  items.c/h   database oggetti + inventario
  quest.c/h   missioni e dialoghi generati dallo stato di gioco
  ui.c/h      HUD, menu, inventario, diario, mappa, negozio
  save.c/h    salvataggio binario
  game.c/h    stato globale, ciclo di gioco, combattimento, disegno
  main.c      finestra e loop principale
docs/         approfondimenti didattici
tools/        script per generare o scaricare asset opzionali
vendor/
  raylib/     submodule: sorgenti di raylib 5.5 (l'unica dipendenza)
```

**Dipendenze**: solo raylib e la libreria standard C (`math.h`, `stdio.h`,
`string.h`, `stdlib.h`, `stdarg.h`). Nessun'altra libreria, nessun C++.

Il file da leggere per primo è `src/world.c`: l'idea centrale del progetto è che
l'altezza del terreno sia una **funzione pura** `WorldHeight(seed, x, z)`.
Mesh, collisioni, minimappa, posizionamento dei villaggi e spawn dei nemici
interrogano tutti la stessa funzione: non esiste una mappa in memoria da tenere
sincronizzata, e il salvataggio può contenere solo il seme.

---

## Documentazione

- `docs/01-architettura.md` — come sono divisi i moduli e perché
- `docs/02-generazione-mondo.md` — il rumore, i biomi, e come usare **heightmap
  esterne** prodotte con QGIS/GDAL, Blender o GIMP
- `docs/03-asset-pubblici.md` — dove prendere asset CC0 e come innestarli
- `docs/04-esercizi.md` — 12 esercizi progressivi, dal più semplice al più tosto

---

## Licenza

Codice: **MIT** (vedi `LICENSE`). raylib è distribuita con licenza zlib/libpng.
Gli asset eventualmente scaricati mantengono la propria licenza: usare solo
sorgenti CC0 / pubblico dominio come indicato in `docs/03-asset-pubblici.md`.
