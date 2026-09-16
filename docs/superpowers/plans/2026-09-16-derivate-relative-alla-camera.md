# Le derivate su una posizione relativa alla camera — piano

> **Per chi esegue:** i passi hanno la casella `- [ ]` per essere spuntati.
> Si esegue **in linea**, in questa sessione, con `superpowers:executing-plans`.
> Niente subagenti: l'utente lo ha chiesto esplicitamente.

**Obiettivo:** togliere la dipendenza della terna tangente dalla precisione
fp32 della posizione assoluta di mondo, costruendo le derivate di schermo su
una posizione **relativa alla camera** calcolata nei vertex shader.

**Architettura:** i due vertex shader smettono di emettere la posizione
assoluta e ne emettono una relativa alla camera, costruita **senza mai formare
il numero grande** — la traslazione meno `viewPos` si somma alla posizione
locale, invece di sottrarre `viewPos` da un `world` già arrotondato. Il varying
cambia nome da `fragPosition` a `fragPosRel`, così ogni uso rimasto indietro non
compila. Il fragment shader ricostruisce l'assoluta con una addizione sola dove
serve davvero, cioè per le tre ricerche d'ombra.

**Tecnologie:** GLSL 330, raylib 5.5, C99. Il banco di misura è una procedura,
non un bersaglio del Makefile: vedi `tools/banco/README.md`.

**Spec:** `docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md`

## Vincoli globali

- **`make` deve restare a zero avvisi**, su Linux e su Windows.
- **Nessuna riga di C nel gioco.** `viewPos` è già un uniform impostato su
  entrambi i programmi in `src/light.c:464`, e un uniform è del programma
  linkato, non dello stadio. Se il piano portasse a toccare `src/`, qualcosa è
  stato capito male: fermarsi e dirlo.
- **La strumentazione non entra mai in `src/`.** Nemmeno dietro una `#define`
  spenta. Vive in copie dei sorgenti fuori dal repository, e nel repository
  entrano solo i **diff**.
- **La condizione d'arresto del Task 3 è vincolante.** Se i due PNG di oggi
  risultano identici, gli shader **non si toccano** e il lavoro finisce al
  Task 3bis.
- **Il costo non si misura.** La previsione è a somma nulla ed è scritta nella
  spec; nessun giro del banco in modalità costo.
- **Le tangenti morte non si toccano.** `BuildTangents()`, `vertexTangent`,
  `fragTangent` restano dove sono, anche se si sta lavorando negli stessi tre
  file. `docs/06` lega la loro rimozione alla domanda C.
- `$SCRATCH` è la cartella scratchpad di questa sessione:
  `/tmp/claude-1000/-home-gomutako-progetti-Frostmark/525ec4b2-385e-4c7a-a410-fcc7c2da4c72/scratchpad`.
  Chi esegue in un'altra sessione ne usa la propria: niente di quello che ci
  finisce dentro deve entrare nel repository.
- Tutti i messaggi di commit finiscono con
  `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.

---

### Task 1: Il confronto fra due PNG

Lo strumento che dice se due fotogrammi differiscono e di quanto. Va scritto
**per primo** perché è l'unico pezzo provabile senza GPU, e perché il Task 3 non
può decidere niente senza di lui.

**File:**
- Creare: `tools/banco/confronta_png.py`
- Prova: `$SCRATCH/prova_confronta.py` (fuori dal repo: è una prova dello
  strumento di misura, non del gioco)

**Interfacce:**
- Produce: un eseguibile `confronta_png.py A.png B.png` che stampa una riga
  sola con gli stessi quattro nomi della modalità rumore —
  `diff_media=<float> pixel=<int> frazione_cambiati=<float> diff_max=<float>` —
  e esce 0. Esce 2 con un messaggio su stderr se gli argomenti sono sbagliati o
  le due immagini hanno dimensioni diverse.
- Consuma: `numpy` e `PIL`, entrambi già presenti su questa macchina
  (verificato: `python3 -c "import PIL, numpy"` passa).

- [ ] **Passo 1: scrivere la prova che fallisce**

Nello scratchpad, `prova_confronta.py`:

```python
#!/usr/bin/env python3
"""Prova di tools/banco/confronta_png.py. Non sta nel repo: prova lo
strumento di misura, non il gioco."""
import subprocess, sys, tempfile, os
import numpy as np
from PIL import Image

SCRIPT = "tools/banco/confronta_png.py"

def scrivi(path, arr):
    Image.fromarray(arr.astype(np.uint8), "RGB").save(path)

def esegui(a, b):
    r = subprocess.run([sys.executable, SCRIPT, a, b],
                       capture_output=True, text=True)
    return r.returncode, r.stdout.strip(), r.stderr.strip()

def campi(riga):
    return dict(kv.split("=") for kv in riga.split())

fallite = 0
def ok(nome, cond):
    global fallite
    print(("ok   " if cond else "FALL ") + nome)
    if not cond: fallite += 1

with tempfile.TemporaryDirectory() as d:
    a = os.path.join(d, "a.png"); b = os.path.join(d, "b.png")
    base = np.full((4, 4, 3), 100, dtype=np.uint8)

    # 1. identiche: tutti i numeri a zero
    scrivi(a, base); scrivi(b, base)
    rc, out, err = esegui(a, b)
    ok("identiche: esce 0", rc == 0)
    c = campi(out) if rc == 0 else {}
    ok("identiche: diff_media == 0", c.get("diff_media") == "0.00000")
    ok("identiche: diff_max == 0",   c.get("diff_max") == "0.00")
    ok("identiche: frazione_cambiati == 0",
       c.get("frazione_cambiati") == "0.00000")
    ok("identiche: pixel == 16", c.get("pixel") == "16")

    # 2. un pixel, canale verde +10: luminanza 0.7152*10 = 7.152 su 16 pixel
    mod = base.copy(); mod[0, 0, 1] = 110
    scrivi(b, mod)
    rc, out, err = esegui(a, b)
    c = campi(out) if rc == 0 else {}
    ok("un pixel: diff_media == 0.44700", c.get("diff_media") == "0.44700")
    ok("un pixel: diff_max == 7.15",      c.get("diff_max") == "7.15")
    ok("un pixel: frazione_cambiati == 0.06250",
       c.get("frazione_cambiati") == "0.06250")

    # 3. dimensioni diverse: errore, non un numero inventato
    scrivi(b, np.full((4, 5, 3), 100, dtype=np.uint8))
    rc, out, err = esegui(a, b)
    ok("dimensioni diverse: esce 2", rc == 2)
    ok("dimensioni diverse: lo dice su stderr", "dimensioni" in err)

    # 4. argomenti sbagliati
    r = subprocess.run([sys.executable, SCRIPT, a], capture_output=True, text=True)
    ok("un argomento solo: esce 2", r.returncode == 2)

print("FALLITE:" , fallite)
sys.exit(1 if fallite else 0)
```

- [ ] **Passo 2: lanciarla e vederla fallire**

```bash
cd /home/gomutako/progetti/Frostmark
python3 "$SCRATCH/prova_confronta.py"
```

Atteso: fallisce su tutto, perché `tools/banco/confronta_png.py` non esiste.
`subprocess.run` di un file inesistente dà `returncode` 2 da Python stesso, e i
casi 3 e 4 potrebbero passare **per la ragione sbagliata**: è normale e il
Passo 4 li rimette in riga. Quello che conta qui è vedere fallire i casi 1 e 2.

- [ ] **Passo 3: scrivere lo strumento**

`tools/banco/confronta_png.py`:

```python
#!/usr/bin/env python3
"""Confronta due fotogrammi del banco sulla luminanza.

Serve alla modalita' fp32: due inquadrature che in aritmetica esatta sarebbero
identiche e differiscono solo per dove stanno nel mondo. I quattro numeri
stampati hanno gli STESSI NOMI della modalita' rumore, perche' misurano la
stessa cosa su coppie diverse - li' fotogrammi consecutivi, qui due posizioni
di mondo - e nomi diversi per la stessa grandezza costringono chi legge a
tenere a mente una tabella di traduzione.

La luminanza e' quella di sempre: 0,2126 R + 0,7152 G + 0,0722 B.
"""
import sys

import numpy as np
from PIL import Image


def luminanza(path):
    a = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def main(argv):
    if len(argv) != 3:
        print("uso: confronta_png.py A.png B.png", file=sys.stderr)
        return 2

    a = luminanza(argv[1])
    b = luminanza(argv[2])
    if a.shape != b.shape:
        print("dimensioni diverse: %s contro %s" % (a.shape, b.shape),
              file=sys.stderr)
        return 2

    d = np.abs(a - b)
    # 'frazione_cambiati' conta i pixel che differiscono di almeno mezzo
    # livello su 255: la stessa soglia della modalita' rumore, ed e' li' che
    # sta scritto il perche' - sotto mezzo livello il confronto misura
    # l'arrotondamento a 8 bit dello schermo, non il fenomeno.
    print("diff_media=%.5f pixel=%d frazione_cambiati=%.5f diff_max=%.2f"
          % (d.mean(), d.size, float((d >= 0.5).sum()) / d.size, d.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

- [ ] **Passo 4: rilanciare la prova e vederla passare**

```bash
python3 "$SCRATCH/prova_confronta.py"
```

Atteso: tutte `ok`, `FALLITE: 0`, esce 0.

- [ ] **Passo 5: sabotare la prova, o non è una prova**

Questo progetto verifica le prove sabotandole, e `docs/06` elenca cinque modi
in cui una prova passa senza provare niente. Qui il sabotaggio è questo:
in `confronta_png.py`, sostituire `d.mean()` con `d.max()` nella riga di stampa
e rilanciare. Deve fallire il caso "un pixel: diff_media == 0.44700" (che
diventerebbe 7,152) e **non** il caso "identiche", che con entrambe le formule
vale zero.

Se il caso "identiche" fosse l'unico presente, la prova non distinguerebbe una
media da un massimo: è lo stesso difetto del *gradino nullo per costruzione*
di `docs/06`. Rimettere `d.mean()` e rilanciare.

- [ ] **Passo 6: committare**

```bash
chmod +x tools/banco/confronta_png.py
git add tools/banco/confronta_png.py
git commit -F - <<'MSG'
Due PNG del banco si confrontano con uno strumento, non a occhio

La modalita' fp32 che arriva confronta due inquadrature che in
aritmetica esatta sarebbero identiche. Serve un numero, e serve che
abbia gli stessi nomi della modalita' rumore: diff_media, pixel,
frazione_cambiati, diff_max misurano la stessa cosa su coppie diverse -
li' fotogrammi consecutivi, qui due posizioni di mondo.

Provato fuori dal repo su immagini costruite a mano, e il sabotaggio -
media scambiata per massimo - lo prende il caso col pixel modificato,
non quello con le immagini identiche.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
MSG
```

---

### Task 2: La terza modalità del banco

Due diff di strumentazione nuovi e una sezione di README. Alla fine di questo
task esistono due PNG prodotti dal binario **di oggi**, non ancora confrontati.

**File:**
- Creare: `tools/banco/strumentazione-fp32-main.c.diff`
- Creare: `tools/banco/strumentazione-fp32-light.c.diff`
- Modificare: `tools/banco/README.md` (una sezione nuova dopo *Le due
  modalità*, che diventano tre)
- Copia di lavoro: `/tmp/banco/fp32/` — **fuori dal repository**

**Interfacce:**
- Consuma: `tools/banco/confronta_png.py` dal Task 1.
- Produce: un binario che con `FROSTMARK_BANCO=fp32` legge
  `FROSTMARK_FP32_POS="x,z"`, `FROSTMARK_FP32_DIST="<metri>"` e
  `FROSTMARK_SCATTO="<file.png>"`, disegna un solo masso e salva un PNG.

- [ ] **Passo 1: costruire la copia dei sorgenti**

```bash
mkdir -p /tmp/banco/fp32
git archive HEAD | tar -x -C /tmp/banco/fp32
cp -r assets/models assets/textures assets/fonts assets/audio /tmp/banco/fp32/assets/
cp assets/heightmap.png /tmp/banco/fp32/assets/
ln -s /home/gomutako/progetti/Frostmark/vendor/raylib /tmp/banco/fp32/vendor/raylib
```

`git archive` porta solo ciò che è tracciato: `assets/models/`,
`assets/textures/`, `assets/fonts/`, `assets/audio/`, `assets/heightmap.png` e
`vendor/raylib` sono esclusi e vanno messi a mano. `assets/world/` è tracciato e
arriva da sé. Se una delle cartelle non esiste nella checkout, saltarla — il
`cp` fallirebbe e il resto no.

- [ ] **Passo 2: modificare `light.c` nella copia — spegnere le ombre**

In `/tmp/banco/fp32/src/light.c`, dentro `LightFrame()`, subito dopo
`int on = (gMap[0].depth.id > 0) ? 1 : 0;`:

```c
    /* BANCO fp32: le ombre vanno spente, e non e' pulizia. Le matrici
     * lightVP dipendono dalla posizione ASSOLUTA nel mondo, quindi la stessa
     * geometria a 64,64 e a 4032,4032 riceve ombra diversa: sarebbe una
     * differenza vera fra i due fotogrammi, e maschererebbe quella cercata,
     * che vale frazioni di livello. */
    {
        static int bancoFp32 = -1;
        if (bancoFp32 < 0) {
            const char *m = getenv("FROSTMARK_BANCO");
            bancoFp32 = (m != NULL && strcmp(m, "fp32") == 0) ? 1 : 0;
        }
        if (bancoFp32) on = 0;
    }
```

Se `light.c` non include già `<stdlib.h>` e `<string.h>`, aggiungerli in cima
alla copia.

- [ ] **Passo 3: modificare `main.c` nella copia — la modalità fp32**

Accanto alle costanti del banco già presenti:

```c
/* ---- Modalita' FP32 -------------------------------------------------------
 * Un solo masso, disegnato due volte alla stessa distanza e con lo stesso
 * sguardo, in due punti opposti del mondo. Tutto e' relativo al masso, quindi
 * in aritmetica esatta i due PNG sarebbero identici bit per bit: cio' che li
 * separa e' solo fp32.
 *
 * Niente terreno, niente entita', niente ombre: a 64,64 e a 4032,4032 la quota
 * del terreno e le matrici della luce sono DIVERSE, e sono differenze vere che
 * coprirebbero quella cercata. */
#define BANCO_FP32_SCALDA  5       /* fotogrammi buttati prima dello scatto */
#define BANCO_FP32_ALTO    1.1f    /* meta' altezza del masso, in metri      */
```

E il ramo nel `main()`, accanto a quelli di costo e rumore:

```c
    } else if (modo != NULL && strcmp(modo, "fp32") == 0) {
        float px = 64.0f, pz = 64.0f, dist = 1.0f;
        const char *sp = getenv("FROSTMARK_FP32_POS");
        if (sp != NULL) sscanf(sp, "%f,%f", &px, &pz);
        const char *sd = getenv("FROSTMARK_FP32_DIST");
        if (sd != NULL) dist = (float)atof(sd);

        World *w = &game.world;
        if (!w->hasExtProp[PROP_ROCK] || w->propVar[PROP_ROCK].n <= 0) {
            printf("fp32: nessun modello esterno per PROP_ROCK\n");
            GameShutdown(&game); CloseWindow(); return 1;
        }

        /* La quota e' FISSA, non WorldHeight(): il terreno e' diverso nei due
         * punti e il masso si troverebbe a due altezze diverse rispetto alla
         * camera, che e' esattamente la differenza che non vogliamo. */
        Vector3 pos = { px, 0.0f, pz };

        Camera3D cam = { 0 };
        cam.position   = (Vector3){ px, BANCO_FP32_ALTO,
                                    pz + BANCO_FP32_ALTO + dist };
        cam.target     = (Vector3){ px, BANCO_FP32_ALTO, pz };
        cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
        cam.fovy       = 70.0f;
        cam.projection = CAMERA_PERSPECTIVE;

        Vector3 sole = Vector3Normalize((Vector3){ 0.35f, 0.70f, 0.45f });
        LightSetSun(sole, 1.0f);
        LightSetProjection(0, 1.0f);
        LightSetAlphaCut(0.0f);

        PropVariants *pv = &w->propVar[PROP_ROCK];
        Model *mo = &w->extProp[PROP_ROCK];
        /* Variante FISSA a 0, non PropVariantOf(): quella sceglie dalla
         * POSIZIONE, quindi a 64,64 e a 4032,4032 disegnerebbe due massi
         * DIVERSI e il confronto misurerebbe la differenza fra due modelli.
         * E' la trappola piu' facile di questa modalita'. */
        float k = pv->scala[0];
        Matrix mt = MatrixMultiply(MatrixScale(k, k, k),
                                   MatrixTranslate(pos.x, pos.y, pos.z));

        for (int f = 0; f <= BANCO_FP32_SCALDA; f++) {
            BeginDrawing();
                ClearBackground(BLACK);
                LightFrame(cam);
                BeginMode3D(cam);
                    for (int j = 0; j < pv->gruppo[0].count; j++) {
                        int mi  = pv->meshIdx[pv->gruppo[0].first + j];
                        int mat = (mo->meshMaterial != NULL)
                                    ? mo->meshMaterial[mi] : 0;
                        if (mat < 0 || mat >= mo->materialCount) mat = 0;
                        DrawMesh(mo->meshes[mi], mo->materials[mat], mt);
                    }
                EndMode3D();
                rlDrawRenderBatchActive();
                if (f == BANCO_FP32_SCALDA) {
                    Image im = LoadImageFromScreen();
                    if (im.data != NULL) {
                        const char *scatto = getenv("FROSTMARK_SCATTO");
                        ExportImage(im, scatto != NULL ? scatto
                                                       : "banco_fp32.png");
                        printf("fp32: pos=%.1f,%.1f dist=%.2f %dx%d\n",
                               px, pz, dist, im.width, im.height);
                        UnloadImage(im);
                    }
                }
            EndDrawing();
        }
        fflush(stdout);
    }
```

Serviranno in cima al file, se non ci sono già: `#include "raymath.h"`,
`#include "light.h"`, `#include "world.h"`, `#include <string.h>`,
`#include <stdlib.h>`. `rlgl.h` c'è già dal diff del rumore.

- [ ] **Passo 4: compilare e lanciare le due esecuzioni di oggi**

```bash
cd /tmp/banco/fp32 && make 2>&1 | tail -20
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=64,64     FROSTMARK_FP32_DIST=1.0 \
  FROSTMARK_SCATTO=/tmp/banco/oggi_vicino_1m.png  ./frostmark
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=4032,4032 FROSTMARK_FP32_DIST=1.0 \
  FROSTMARK_SCATTO=/tmp/banco/oggi_lontano_1m.png ./frostmark
```

Atteso: due righe `fp32: pos=... 1280x720` e due PNG.

- [ ] **Passo 5: guardare i due PNG prima di misurarli**

Aprirli e controllare che il masso **riempia il fotogramma**. È la trappola
scritta nella spec: se il masso occupa cento pixel su un milione, la differenza
media è divisa per diecimila e sembra zero. Se non riempie, correggere
`BANCO_FP32_ALTO` o `dist` e rifare il Passo 4 — **in entrambe le esecuzioni**,
o le due inquadrature non sono più confrontabili.

Controllare anche che i due PNG **non siano neri**: se il masso è sotto il
terreno immaginario o fuori campo, la modalità misurerebbe due schermate vuote
identiche, e il Task 3 concluderebbe "nessun difetto" per la ragione sbagliata.
Questa è la forma locale della *prova che non prende niente*.

- [ ] **Passo 6: generare i due diff e versionarli**

```bash
cd /home/gomutako/progetti/Frostmark
diff -u src/main.c  /tmp/banco/fp32/src/main.c  > tools/banco/strumentazione-fp32-main.c.diff
diff -u src/light.c /tmp/banco/fp32/src/light.c > tools/banco/strumentazione-fp32-light.c.diff
```

`diff -u` esce 1 quando i file differiscono, ed è il caso normale qui: non è un
errore.

**Attenzione:** il diff di `main.c` conterrà *anche* le modifiche dei due diff
esistenti, perché la copia le ha addosso. Va bene ed è il motivo per cui il file
si chiama `strumentazione-fp32-main.c.diff` e non `-fp32-solo-`: è un diff
**completo** dal `main.c` del repo, e si applica da solo, senza gli altri due.
Scriverlo nel README, o chi riprende applicherà tre diff sovrapposti.

- [ ] **Passo 7: aggiornare `tools/banco/README.md`**

Tre modifiche:

1. il titolo della sezione *Le due modalità* diventa *Le tre modalità*;
2. una sottosezione nuova `### FROSTMARK_BANCO=fp32 — modalità **precisione**`
   che dice: cosa disegna (un masso solo), cosa spegne e **perché** (terreno,
   entità e ombre sono differenze vere fra i due posti), le tre variabili
   d'ambiente, le due trappole — il masso che non riempie il fotogramma, e la
   variante scelta dalla posizione — e che il diff di `main.c` è completo e non
   si somma agli altri due;
3. nella sezione *Come si costruisce*, una riga che dice che la modalità fp32
   vuole **due** diff, `-fp32-main.c` e `-fp32-light.c`, e non quelli delle
   altre due modalità.

- [ ] **Passo 8: committare**

```bash
git add tools/banco/strumentazione-fp32-main.c.diff \
        tools/banco/strumentazione-fp32-light.c.diff tools/banco/README.md
git commit -F - <<'MSG'
Il banco impara a misurare la precisione, non solo il tempo

Le due modalita' esistenti non rispondono alla domanda della regressione
fp32: costo misura millisecondi, rumore misura la varianza fra
fotogrammi consecutivi. Qui servono due fotogrammi che in aritmetica
esatta sarebbero identici e differiscono solo per dove stanno nel mondo.

La modalita' disegna un masso solo e spegne terreno, entita' e ombre.
Non e' pulizia: la quota del terreno e le matrici della luce sono
DIVERSE a 64,64 e a 4032,4032, e coprirebbero la differenza cercata.

Le due trappole stanno nel README e nei commenti: un masso che non
riempie il fotogramma divide la differenza per diecimila, e la variante
scelta da PropVariantOf() disegnerebbe due massi diversi nei due punti.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
MSG
```

---

### Task 3: Misurare il difetto, e la condizione d'arresto

**Questo task può terminare il lavoro.** Va eseguito prima di toccare un solo
carattere di GLSL.

**File:** nessuno nel repo fino al Passo 4.

**Interfacce:**
- Consuma: i PNG del Task 2 e `confronta_png.py` del Task 1.
- Produce: quattro numeri — la coppia a 1 m e la coppia a 0,5 m — e la
  decisione se proseguire.

- [ ] **Passo 1: la coppia a un metro**

```bash
cd /home/gomutako/progetti/Frostmark
python3 tools/banco/confronta_png.py /tmp/banco/oggi_vicino_1m.png \
                                     /tmp/banco/oggi_lontano_1m.png
```

- [ ] **Passo 2: la coppia a mezzo metro**

```bash
cd /tmp/banco/fp32
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=64,64     FROSTMARK_FP32_DIST=0.5 \
  FROSTMARK_SCATTO=/tmp/banco/oggi_vicino_05m.png  ./frostmark
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=4032,4032 FROSTMARK_FP32_DIST=0.5 \
  FROSTMARK_SCATTO=/tmp/banco/oggi_lontano_05m.png ./frostmark
cd /home/gomutako/progetti/Frostmark
python3 tools/banco/confronta_png.py /tmp/banco/oggi_vicino_05m.png \
                                     /tmp/banco/oggi_lontano_05m.png
```

- [ ] **Passo 3: leggere il risultato contro la condizione dichiarata**

La spec dichiara la condizione d'arresto, e qui si applica senza negoziare:

| esito | cosa si fa |
|---|---|
| `diff_media` **maggiore di zero** a un metro o a mezzo metro | il difetto esiste. Si annota il numero e si va al **Task 4** |
| `diff_media == 0` e `diff_max == 0` in **entrambe** le coppie | il difetto è teorico. Si va al **Task 3bis** e il lavoro finisce lì |

Prima di concludere "zero", ricontrollare il Passo 5 del Task 2: due schermate
vuote danno zero e non vogliono dire niente.

- [ ] **Passo 4: annotare la misura nello scratchpad**

Scrivere i quattro numeri in
`$SCRATCH/misure-fp32.md` — servono ai Task 5 e 6 e non devono vivere solo
nello scrollback del terminale.

---

### Task 3bis: SOLO se il difetto non c'è

Da eseguire **solo** se il Passo 3 del Task 3 ha dato zero su entrambe le
coppie. Altrimenti saltare al Task 4.

**File:**
- Modificare: `docs/06-stato-e-prossimi-passi.md`, domanda **F**
- Modificare: `docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md`

- [ ] **Passo 1: scrivere il risultato in `docs/06`**

Nella domanda **F**, il paragrafo che si apre con *«La regressione, ed è l'unica
vera»* va riscritto: il conto degli ulp resta com'è — è giusto — ma si aggiunge
che la verifica **è stata fatta**, con quale banco, e che a un metro e a mezzo
metro i due fotogrammi sono identici. Dire anche il limite: il banco misura un
masso, non un volto animato, quindi l'assenza è misurata su quel caso.

E togliere la regressione dalla riga della tabella *le tangenti morte*, che oggi
dice che la F lascia dietro **due** lavori: ne resterebbe uno.

- [ ] **Passo 2: chiudere la spec**

In coda alla spec, una sezione *Esito* con i quattro numeri e la frase che gli
shader non sono stati toccati, perché una spec che descrive un lavoro non fatto
è peggio di nessuna spec.

- [ ] **Passo 3: committare e fermarsi**

```bash
git add docs/06-stato-e-prossimi-passi.md \
        docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md
git commit -F - <<'MSG'
La regressione fp32 non si manifesta, e il conto era giusto lo stesso

Misurata col banco in modalita' precisione: lo stesso masso a un metro e
a mezzo metro, a 64,64 e a 4032,4032, da' due fotogrammi identici. Il
conto degli ulp resta valido - a 3000 m un ulp vale un terzo del passo
di mondo per pixel - ma sul caso misurato l'errore non arriva a mezzo
livello su 255.

Gli shader non sono stati toccati. Il limite della misura e' scritto: un
masso non e' un volto animato.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
MSG
```

---

### Task 4: La correzione dei tre shader

Da eseguire solo se il Task 3 ha trovato il difetto.

**File:**
- Modificare: `assets/shaders/scene.vs`
- Modificare: `assets/shaders/scene_inst.vs`
- Modificare: `assets/shaders/scene.fs`

**Interfacce:**
- Produce: il varying `fragPosRel`, posizione **relativa alla camera** in metri,
  emesso da entrambi i vertex shader e letto dal fragment. `fragPosition` non
  esiste più in nessuno dei tre file.
- Consuma: `uniform vec3 viewPos`, già impostato da `src/light.c:464` su
  entrambi i programmi. Nessuna riga di C.

- [ ] **Passo 1: `scene.vs`**

Sostituire `out vec3 fragPosition;` con:

```glsl
/* La posizione RELATIVA ALLA CAMERA, in metri. Non e' un dettaglio di
 * comodo: scene.fs ne prende le derivate di schermo per costruire la terna
 * della normal map, e in fp32 un ulp di una coordinata assoluta a 3000 m
 * vale 3,6e-4 m, cioe' un terzo del passo di mondo fra due pixel a un metro
 * di distanza. Assoluta, la terna arriva con un errore del 15-30% proprio
 * dove serve: un volto a un metro. Vedi docs/06, domanda F. */
out vec3 fragPosRel;
```

Aggiungere fra gli uniform:

```glsl
uniform vec3 viewPos;    /* la camera. light.c la mette su entrambi i
                          * programmi, e un uniform e' del programma linkato:
                          * dichiararla qui non costa una riga di C. */
```

E nel corpo, al posto di `fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));`:

```glsl
    /* Non si calcola la posizione di mondo per poi sottrarre la camera: a
     * quel punto il numero grande e' GIA' arrotondato, e la sottrazione -
     * per quanto esatta, i due operandi sono vicini - conserva l'errore.
     * Si somma invece la posizione locale (metri) a una differenza fra
     * posizioni grandi e VICINE, che in fp32 e' esatta o quasi. matModel e'
     * affine, quindi mat3() ne prende la parte lineare e [3].xyz la
     * traslazione. */
    vec3 local  = mat3(matModel) * vertexPosition;
    fragPosRel  = local + (matModel[3].xyz - viewPos);
```

`gl_Position` non si tocca.

- [ ] **Passo 2: `scene_inst.vs`**

Stesso uniform e stesso varying rinominato, con lo stesso commento. Nel corpo,
la riga di `world` si spezza in due e se ne aggiunge una terza:

```glsl
    vec3 local = RuotaY(vertexPosition * sc, s, c);
    vec3 world = local + instPosSin.xyz;   /* serve ancora a gl_Position */
```

e al posto di `fragPosition = world;`:

```glsl
    /* Stessa regola di scene.vs: mai formare il numero grande. Qui la
     * traslazione e' la posizione dell'istanza. */
    fragPosRel = local + (instPosSin.xyz - viewPos);
```

- [ ] **Passo 3: `scene.fs`**

Quattro modifiche:

1. `in vec3 fragPosition;` diventa `in vec3 fragPosRel;`, con una riga di
   commento che dice che è relativa alla camera e rimanda a `docs/06`;
2. in `SurfaceNormal()`:

```glsl
    /* Relativa alla camera, non assoluta: e' il punto di tutto il lavoro del
     * 16 settembre. Assoluta, a 3000 m un ulp vale un terzo del passo di
     * mondo fra due pixel, e la derivata e' la differenza di due valori
     * ciascuno gia' sporco. */
    vec3 dp1 = dFdx(fragPosRel);
    vec3 dp2 = dFdy(fragPosRel);
```

3. in `ShadowFactor()`, la distanza e la ricostruzione, in cima alla funzione
   subito dopo `float ndl = ...`:

```glsl
    float dist = length(fragPosRel);

    /* Le matrici della luce vivono in coordinate di MONDO, quindi qui
     * l'assoluta serve davvero. Si ricostruisce una volta sola e non tre:
     * la sua precisione e' quella di prima, ne' meglio ne' peggio, perche'
     * a questo punto il numero grande si riforma comunque. */
    vec3 wp = fragPosRel + viewPos;
```

e le tre `vec4(fragPosition, 1.0)` diventano `vec4(wp, 1.0)`;

4. il commentone sopra `SurfaceNormal()` nomina `fragPosition` una volta
   (riga 85 circa): aggiornare il nome **e** aggiungere che la posizione è
   relativa alla camera per la ragione fp32.

- [ ] **Passo 4: compilare e far girare il gioco**

```bash
cd /home/gomutako/progetti/Frostmark && make 2>&1 | tail -20
```

Atteso: zero avvisi. Poi lanciare `./frostmark` e **guardare la scena**: gli
shader si compilano a runtime, quindi un errore GLSL non lo prende `make` — lo
prende il registro. Se `LightInit()` fallisce, raylib stampa l'errore di
compilazione e il gioco disegna senza shader.

Su pipe l'uscita del registro è bufferizzata: redirigerla su file o si legge
una schermata vuota.

```bash
./frostmark > /tmp/banco/registro.txt 2>&1 &
sleep 8; kill %1
grep -i "shader\|error\|warn" /tmp/banco/registro.txt | head -20
```

- [ ] **Passo 5: `make prove`**

```bash
make prove
```

Atteso: le dieci prove passano. `normalmap.c` e `luce.c` controllano che gli
uniform esistano, `viewPos` compreso: se il rinominare avesse rotto il link fra
i due stadi, `LightInit()` fallirebbe e le prove con contesto lo direbbero.

Le prove che non trovano un contesto OpenGL escono 77 e contano come saltate:
non è un fallimento.

- [ ] **Passo 6: `make valida`**

```bash
make valida
```

Atteso: nessun problema. Questo lavoro non tocca i dati né il mondo cotto, e
serve a dirlo con un comando invece che con un'opinione.

- [ ] **Passo 7: committare**

```bash
git add assets/shaders/scene.vs assets/shaders/scene_inst.vs assets/shaders/scene.fs
git commit -F - <<'MSG'
Le derivate si prendono su una posizione relativa alla camera

Il varying che scene.fs deriva era assoluto in metri-mondo, fino a 4096.
In fp32 un ulp a 3000 m vale 3,6e-4 m, un terzo del passo di mondo fra
due pixel a un metro: la terna della normal map arrivava con un errore
del 15-30% proprio nel caso per cui esiste, un volto vicino.

Due cose che sembrano equivalenti e non lo sono. Sottrarre viewPos nel
fragment non ripara niente, perche' i bit si perdono interpolando il
varying. E nemmeno world - viewPos basta: world e' gia' arrotondato, e
la sottrazione esatta conserva l'errore. La relativa si costruisce
sommando la posizione locale a una differenza fra posizioni vicine.

Cosi' l'errore residuo diventa per vertice invece che per frammento, e
l'interpolazione lineare lo rende una pendenza costante che la derivata
non vede, invece di rumore scorrelato pixel per pixel.

Il varying cambia nome apposta: un uso rimasto indietro non compila
invece di sbagliare in silenzio.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
MSG
```

---

### Task 5: I due confronti che dicono se ha funzionato

**File:** nessuno nel repo. Copia di lavoro: `/tmp/banco/fp32dopo/`.

**Interfacce:**
- Consuma: i PNG di oggi del Task 3, i diff del Task 2, gli shader del Task 4.
- Produce: i due numeri che chiudono la verifica.

- [ ] **Passo 1: costruire la copia corretta**

```bash
mkdir -p /tmp/banco/fp32dopo
git archive HEAD | tar -x -C /tmp/banco/fp32dopo
cp -r assets/models assets/textures assets/fonts assets/audio /tmp/banco/fp32dopo/assets/
cp assets/heightmap.png /tmp/banco/fp32dopo/assets/
ln -s /home/gomutako/progetti/Frostmark/vendor/raylib /tmp/banco/fp32dopo/vendor/raylib
cd /tmp/banco/fp32dopo
patch -p1 < tools/banco/strumentazione-fp32-main.c.diff
patch -p1 < tools/banco/strumentazione-fp32-light.c.diff
make 2>&1 | tail -5
```

`HEAD` ora contiene gli shader corretti, quindi questa copia è "dopo". La copia
`/tmp/banco/fp32/` resta "oggi" e **non va ricompilata**: è il termine di
paragone.

Gli shader sono file di testo letti a runtime, quindi i due eseguibili
dovrebbero risultare identici. Controllarlo, perché è il controllo che il banco
ha già usato una volta per dimostrare che l'unica differenza era lo shader:

```bash
cmp /tmp/banco/fp32/frostmark /tmp/banco/fp32dopo/frostmark && echo "eseguibili identici"
```

Se differiscono, la differenza misurata non isola più lo shader: fermarsi e
capire perché prima di leggere qualunque numero.

- [ ] **Passo 2: le due esecuzioni corrette**

```bash
cd /tmp/banco/fp32dopo
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=64,64     FROSTMARK_FP32_DIST=1.0 \
  FROSTMARK_SCATTO=/tmp/banco/dopo_vicino_1m.png  ./frostmark
FROSTMARK_BANCO=fp32 FROSTMARK_FP32_POS=4032,4032 FROSTMARK_FP32_DIST=1.0 \
  FROSTMARK_SCATTO=/tmp/banco/dopo_lontano_1m.png ./frostmark
```

- [ ] **Passo 3: i due confronti**

```bash
cd /home/gomutako/progetti/Frostmark
echo "dopo vicino vs dopo lontano  (deve essere zero):"
python3 tools/banco/confronta_png.py /tmp/banco/dopo_vicino_1m.png \
                                     /tmp/banco/dopo_lontano_1m.png
echo "oggi vicino vs dopo vicino   (prende gli errori di segno):"
python3 tools/banco/confronta_png.py /tmp/banco/oggi_vicino_1m.png \
                                     /tmp/banco/dopo_vicino_1m.png
```

| confronto | atteso | se non torna |
|---|---|---|
| dopo vicino **vs** dopo lontano | `diff_media=0.00000` | la correzione non basta: c'è ancora una posizione assoluta da qualche parte nel percorso |
| oggi vicino **vs** dopo vicino | zero o quasi | vicino all'origine relativa e assoluta quasi coincidono: se qui cambia qualcosa non è la precisione, è un `+` diventato `−` o una `local` sbagliata |

Il secondo è il controllo che vale di più, ed è il motivo per cui è in tabella:
è l'unico che prende un errore di segno, che altrimenti passerebbe inosservato
perché anche una terna sbagliata è *stabile* fra i due posti.

- [ ] **Passo 4: annotare le due misure**

In `$SCRATCH/misure-fp32.md`, accanto a quelle del Task 3.

---

### Task 6: Scrivere quello che si è imparato

Tre documenti, un commit.

**File:**
- Modificare: `docs/01-architettura.md`, sezione *Normal map*
- Modificare: `docs/06-stato-e-prossimi-passi.md`, domanda **F** e la riga
  *le tangenti morte* della tabella
- Modificare: `docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md`
- Modificare: `tools/banco/README.md`, sezione *Il precedente*

- [ ] **Passo 1: `docs/01`, sezione *Normal map***

Il paragrafo che oggi dice *«Le derivate lavorano su `fragPosition`, che esce
dal vertex shader dopo `matModel`»* è ora sbagliato nel nome e incompleto nel
contenuto. Riscriverlo con `fragPosRel`, e aggiungere un paragrafo nuovo che
dica le tre cose che un lettore non ricostruisce da solo:

- perché la posizione è relativa alla camera (il conto degli ulp);
- perché sottrarre nel fragment non sarebbe servito;
- perché non basta `world - viewPos`, e perché spostare l'errore **da per
  frammento a per vertice** è la ragione vera per cui funziona.

Nell'elenco dei limiti, il punto sulle UV degeneri resta identico: questo lavoro
non lo tocca.

- [ ] **Passo 2: `docs/06`, domanda F**

Il paragrafo *«La regressione, ed è l'unica vera: fp32 in campo vicino e lontano
dall'origine»* diventa la storia di un difetto **riparato**: il conto resta, e
si aggiunge la misura del Task 3, la correzione e le due verifiche del Task 5.

E la riga della tabella *le tangenti morte* va corretta: oggi dice che la
domanda F lascia dietro **due** lavori. Ne resta uno.

- [ ] **Passo 3: la spec, sezione *Esito***

In coda alla spec, i numeri veri contro quelli previsti: l'ampiezza del difetto
misurata, i due confronti di verifica, e se la previsione a somma nulla sul
costo è rimasta una previsione (lo è: non è stata misurata, ed è scritto).

- [ ] **Passo 4: `tools/banco/README.md`, sezione *Il precedente***

Quella sezione oggi si chiude con *«La stessa spec propone una verifica non
ancora eseguita»*, e descrive proprio questa. Ora è stata eseguita: sostituire
quel paragrafo con il risultato e il rimando alla spec del 16 settembre.

- [ ] **Passo 5: committare**

```bash
git add docs/01-architettura.md docs/06-stato-e-prossimi-passi.md \
        docs/superpowers/specs/2026-09-16-derivate-relative-alla-camera-design.md \
        tools/banco/README.md
git commit -F - <<'MSG'
La regressione fp32 e' misurata, riparata e agli atti

docs/01 non nomina piu' un varying che non esiste, e dice le tre cose
che un lettore non ricostruisce da solo: perche' la posizione e'
relativa alla camera, perche' sottrarre nel fragment non sarebbe
servito, e perche' spostare l'errore da per-frammento a per-vertice e'
la ragione vera per cui la correzione funziona.

docs/06 aggiorna la domanda F: dei due lavori che lasciava dietro ne
resta uno, le tangenti morte, che restano legate alla domanda C.

Il README del banco non promette piu' una verifica non eseguita.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
MSG
```

---

## Cosa questo piano NON fa

- **Non toglie le tangenti morte.** `BuildTangents()`, `vertexTangent`,
  `fragTangent`. Si sta lavorando negli stessi tre file e la tentazione è forte:
  farlo qui renderebbe inattribuibile qualunque differenza il banco mostrasse,
  e `docs/06` lega quella rimozione alla domanda C.
- **Non copre le UV degeneri.** Restano scoperte, con la stessa condizione e lo
  stesso ripiego.
- **Non misura il costo.** La previsione a somma nulla è nella spec.
- **Non aggiunge una prova a `tools/prove/`.** Sarebbe la prima a rileggere il
  framebuffer, e il banco risponde già. Che il banco vada lanciato a mano è un
  limite dichiarato.
