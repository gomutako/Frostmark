# 03 — Asset pubblici

## Premessa: il gioco non ne ha bisogno

Frostmark funziona con **zero file binari**. Texture, terreno, edifici, alberi e
personaggi sono generati a runtime da primitive (`GenMeshCylinder`, `GenMeshCone`,
`GenMeshSphere`, `GenMeshCube`) e da texture procedurali (`MakeGrainTexture`).
Questo è deliberato: un progetto didattico che si scarica e compila senza
dipendere da un CDN, e che non ha problemi di licenza da nessuna parte.

Gli asset esterni sono quindi un **innesto opzionale**. Questo documento spiega
dove prenderli restando nel pubblico dominio e dove esattamente agganciarli.

---

## Fonti CC0 affidabili

CC0 significa rinuncia al diritto d'autore: uso commerciale libero, nessuna
attribuzione obbligatoria (resta comunque buona educazione citare l'autore).

| Fonte | Cosa offre | Licenza |
|---|---|---|
| **kenney.nl/assets** | modelli low-poly, icone UI, font, effetti sonori — il pacchetto migliore per questo progetto | CC0 |
| **ambientcg.com** | texture PBR tileable (erba, roccia, neve, legno, pietra) | CC0 |
| **polyhaven.com** | texture, HDRI, modelli ad alta qualità | CC0 |
| **quaternius.com** | modelli low-poly stilizzati: alberi, edifici, personaggi animati | CC0 |
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
    grass.png  rock.png  snow.png  sand.png     da ambientCG
  models/
    tree.glb  pine.glb  house.glb  rock.glb     da Quaternius / Kenney
  fonts/
    ui.ttf                                       da Google Fonts
  audio/
    ambient_wind.ogg  hit.wav  step.wav          da Kenney / Freesound
```

---

## Punti di innesto nel codice

### 1. Texture del terreno (il più semplice, e quello che si vede di più)

In `world.c`, `WorldInit()`:

```c
w->terrainTex = MakeGrainTexture(seed + 55u, 256);
```

Sostituire con:

```c
if (FileExists("assets/textures/grass.png")) {
    w->terrainTex = LoadTexture("assets/textures/grass.png");
    GenTextureMipmaps(&w->terrainTex);
    SetTextureFilter(w->terrainTex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(w->terrainTex, TEXTURE_WRAP_REPEAT);
} else {
    w->terrainTex = MakeGrainTexture(seed + 55u, 256);
}
```

Nota: i colori dei vertici (bioma + luce pre-calcolata) **moltiplicano** la
texture. Se la texture è già molto colorata il risultato diventa cupo: in quel
caso conviene schiarire i colori di bioma in `WorldBiomeColor()` oppure usare
texture desaturate. Un passo avanti è il *texture splatting* (esercizio 8).

### 2. Modelli al posto delle primitive

In `world.h` la struttura `World` tiene `mCyl, mCone, mSphere, mCube`. Basta
aggiungere modelli veri:

```c
Model mTree, mPine, mHouse;   /* in World */

/* in WorldInit() */
if (FileExists("assets/models/tree.glb")) w->mTree = LoadModel("assets/models/tree.glb");
```

e in `DrawProp()` sostituire il caso `PROP_TREE`:

```c
case PROP_TREE:
    DrawModelEx(w->mTree, pos, (Vector3){0,1,0}, p->rot,
                (Vector3){ s, s, s }, Shade(WHITE, tint));
    break;
```

raylib carica `.glb`/`.gltf`, `.obj`, `.iqm`, `.m3d`. Per i modelli **animati**
servono `LoadModelAnimations()` e `UpdateModelAnimation()` — è il salto di
qualità più visibile per gli NPC (esercizio 10).

Ricordarsi di scaricare i modelli in `WorldUnload()` con `UnloadModel()`.

### 3. Font dell'interfaccia

`ui.c` usa `DrawText()`, che impiega il font di default (bitmap, un po' rigido).
Con un TTF:

```c
/* una volta, all'avvio */
Font uiFont = LoadFontEx("assets/fonts/ui.ttf", 32, NULL, 0);
SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

/* al posto di DrawText(...) */
DrawTextEx(uiFont, testo, (Vector2){ x, y }, size, 1.0f, colore);
```

Conviene metterlo in `Game` e passarlo alle funzioni di `ui.c`, oppure tenerlo in
una variabile `static` del modulo.

### 4. Audio

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
