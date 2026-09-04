# Instancing dei prop — piano d'implementazione

> **Per chi esegue:** usare `superpowers:subagent-driven-development` (consigliato)
> o `superpowers:executing-plans` per eseguire il piano compito per compito. I
> passi usano caselle (`- [ ]`) per tenere il conto.

**Obiettivo:** disegnare i prop e i pezzi d'edificio con una chiamata per gruppo
invece di una per oggetto, su tutti e tre i passaggi, con un percorso di disegno
scritto da noi e un buffer d'istanza persistente.

**Architettura:** un modulo `instancing.c` tiene un *lotto* per coppia (mesh,
materiale), con un VAO proprio e un VBO d'istanza che vive quanto il mondo. Un
secondo vertex shader ricostruisce la matrice del modello e quella delle normali
da posizione, seno/coseno dell'imbardata e scala. Il fragment shader resta uno
solo per entrambi i percorsi.

**Tecnologie:** C99, raylib 5.5, OpenGL 3.3 via `rlgl`, GLSL 330.

**Spec:** `docs/superpowers/specs/2026-09-04-instancing-e-impostori-design.md`

## Vincoli globali

- C99, stesse opzioni del `Makefile`: `-std=c99 -Wall -Wextra`. **Zero avvisi.**
- Nessuna dipendenza nuova. raylib è già in `vendor/`.
- Si compila anche per Windows (`make windows`, mingw): niente di specifico per Linux.
- Commenti e documentazione **in italiano**, come tutto il resto del repo. Nei
  `.c` si usa l'apostrofo al posto degli accenti (`liberta'`), nei `.md` gli
  accenti veri.
- Il gioco deve funzionare **senza** `assets/shaders/`: se lo shader non c'è,
  `LightReady()` è falso e si torna al percorso non instanziato. L'assenza di un
  file non è un errore.
- Ogni tappa si misura prima di passare alla successiva, con lo stesso percorso
  di 75 secondi della baseline.

## Scostamenti dallo spec, decisi qui

Due, entrambi con motivo:

1. **Gli attributi d'istanza vanno agli slot 8 e 9, non 5 e 6.** Lo spec diceva
   "0–4 sono già presi". Verificato in `vendor/raylib/src/rlgl.h:328-352`: raylib
   usa 0=posizione, 1=UV, 2=normale, 3=colore, 4=tangente, 5=UV2, 6=indici, 7 e 8
   per le ossa quando la skinning GPU è accesa. Gli slot 8 e 9 sono liberi in
   ogni configurazione. OpenGL 3.3 ne garantisce almeno 16.

2. **Il dato d'istanza è di 32 byte, non 28.** Lo spec prevedeva `vec4(pos, yaw)`
   + `vec3(scala)` con `sin`/`cos` calcolati nel vertex shader. Seno e coseno si
   calcolano invece **sulla CPU**, una volta per istanza invece che una volta per
   vertice: `vec4(pos, sinYaw)` + `vec4(scala, cosYaw)`. Quattro byte in più su un
   buffer minuscolo, in cambio di qualche milione di `sincos` in meno per
   fotogramma. Resta un terzo dei 100 byte della soluzione ovvia.

---

## Struttura dei file

**Nuovi:**

| File | Responsabilità |
|---|---|
| `src/instancing.h` | API del lotto: creare, riempire, svuotare |
| `src/instancing.c` | VAO proprio, VBO persistente, caricamento e disegno |
| `assets/shaders/scene_inst.vs` | vertex shader instanziato |
| `tools/prove/prova.h` | asserzioni minime condivise dalle prove |
| `tools/prove/scale.c` | prova esistente delle scale solide, resa permanente |
| `tools/prove/normalmap.c` | prova esistente della normal map, resa permanente |
| `tools/prove/instancing.c` | equivalenza instanziato / non instanziato |

**Modificati:**

| File | Cosa cambia |
|---|---|
| `src/light.h`, `src/light.c` | due programmi invece di uno; `LightInstShader()` |
| `src/world.h`, `src/world.c` | lotti per tipo, riempimento nei cicli di disegno |
| `Makefile` | bersaglio `prove` |
| `docs/01-architettura.md` | sezione sull'instancing |

---

## Task 1: la casa delle prove

Il repo non ha un posto per le prove: nella fase 1 ne sono state scritte tre e
sono finite tutte nello scratchpad. Ogni compito successivo di questo piano ha un
ciclo di prova, quindi la casa serve **prima**.

**Files:**
- Create: `tools/prove/prova.h`, `tools/prove/scale.c`, `tools/prove/normalmap.c`
- Modify: `Makefile`

**Interfaces:**
- Produces: `make prove` compila ed esegue ogni `tools/prove/*.c`, esce non-zero
  se una fallisce. `prova.h` espone `Ok(nome, cond)`, `Near(nome, letto, atteso,
  tolleranza)`, `ProveEsito(void)` che ritorna 0 o 1.

Le prove includono direttamente il `.c` sotto esame (`#include "../../src/light.c"`)
perché quasi tutto ciò che vale la pena provare è `static`. Ogni prova ha quindi
la sua riga di collegamento esplicita nel Makefile: gli oggetti che include non
vanno anche collegati, o si duplicano i simboli.

- [ ] **Step 1: scrivere `tools/prove/prova.h`**

```c
/* ============================================================================
 * prova.h - Asserzioni minime per le prove di Frostmark.
 *
 * Non c'e' un framework e non serve: una prova qui e' un eseguibile che stampa
 * una riga per controllo ed esce non-zero se qualcosa non torna. Le prove
 * includono il .c che provano, perche' cio' che vale la pena provare e' quasi
 * sempre 'static'.
 * ========================================================================== */
#ifndef PROVA_H
#define PROVA_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int gProveFallite = 0;

static void Ok(const char *cosa, int cond)
{
    printf("%-52s %s\n", cosa, cond ? "ok" : "FALLITO");
    if (!cond) gProveFallite++;
}

/* Per i valori misurati: un pixel puo' differire di uno fra due GPU. */
static void Near(const char *cosa, int letto, int atteso, int tolleranza)
{
    int c = abs(letto - atteso) <= tolleranza;
    printf("%-52s atteso %3d, letto %3d  %s\n", cosa, atteso, letto,
           c ? "ok" : "FALLITO");
    if (!c) gProveFallite++;
}

static int ProveEsito(void)
{
    printf("\n%s\n", gProveFallite ? "PROVA FALLITA" : "tutto a posto");
    return gProveFallite ? 1 : 0;
}

#endif /* PROVA_H */
```

- [ ] **Step 2: portare le due prove esistenti in `tools/prove/`**

Copiare i due sorgenti già scritti nella fase 1 dallo scratchpad, sostituendo le
funzioni `Ok`/`Near` locali con `#include "prova.h"` e i `return fails ? 1 : 0`
finali con `return ProveEsito();`. I percorsi di inclusione diventano
`#include "../../src/world.c"` e `#include "../../src/light.c"`.

Le prove aprono una finestra **nascosta** perché servono un contesto GL vero:

```c
if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
SetTraceLogLevel(LOG_WARNING);
SetConfigFlags(FLAG_WINDOW_HIDDEN);
InitWindow(64, 64, "prova");
if (!IsWindowReady()) { printf("niente contesto GL: prova saltata\n"); return 77; }
```

- [ ] **Step 3: aggiungere il bersaglio `prove` al Makefile**

```make
# ---- prove ----------------------------------------------------------------
# Ogni prova include il .c che prova, quindi ha la sua riga di collegamento:
# collegare anche l'oggetto darebbe simboli doppi. Vanno lanciate dalla radice
# del repo, perche' caricano gli asset per percorso relativo.
PROVE_DIR  := $(BUILD_DIR)/prove
PROVE_CF   := -std=gnu99 -Wall -Wextra -Itools/prove -Isrc -Itools -Ivendor/raylib/src -O0
PROVE_LIB  := vendor/raylib/src/libraylib.a $(LDLIBS)

.PHONY: prove
prove: $(RAYLIB_DEP)
	@mkdir -p $(PROVE_DIR)
	$(CC) $(PROVE_CF) tools/prove/scale.c     src/fmath.c src/light.c src/worldio.c src/dataparse.c $(PROVE_LIB) -o $(PROVE_DIR)/scale
	$(CC) $(PROVE_CF) tools/prove/normalmap.c src/fmath.c                                           $(PROVE_LIB) -o $(PROVE_DIR)/normalmap
	@ok=1; for t in $(PROVE_DIR)/*; do \
	   echo "== $$t"; \
	   $$t; r=$$?; \
	   if [ $$r = 77 ]; then echo "   (saltata)"; elif [ $$r != 0 ]; then ok=0; fi; \
	 done; \
	 [ $$ok = 1 ] || { echo "PROVE FALLITE"; exit 1; }
	@echo "==> prove passate"
```

- [ ] **Step 4: verificare che passino**

Run: `make prove`
Expected: le due prove stampano tutti `ok`, il bersaglio esce 0.

- [ ] **Step 5: verificare che la casa discrimini**

Run: `git stash && make prove; git stash pop`
Expected: `PROVE FALLITE`, con 3 controlli falliti in `scale` e 8 in `normalmap`.
Se passano tutte, la casa delle prove non serve a niente e va sistemata prima di
proseguire.

- [ ] **Step 6: commit**

```bash
git add tools/prove Makefile
git commit -m "Un posto dove stanno le prove, e le due che c'erano

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 2: due programmi di shader invece di uno

**Files:**
- Create: `assets/shaders/scene_inst.vs`
- Modify: `src/light.c`, `src/light.h`
- Test: `tools/prove/luce.c`

**Interfaces:**
- Consumes: `prova.h` (Task 1).
- Produces: `Shader LightInstShader(void)` — il programma instanziato, `id == 0`
  se la luce non è pronta. `LightFrame()`, `LightShadowBegin()` e
  `LightShadowEnd()` impostano le uniform su **entrambi** i programmi.

Il punto delicato: i due programmi sono due `glProgram` distinti, quindi hanno
**location diverse per la stessa uniform**. `light.c` passa da variabili singole
a due insiemi paralleli indicizzati per programma.

- [ ] **Step 1: scrivere la prova, che deve fallire**

`tools/prove/luce.c`, sulla falsariga di `normalmap.c`:

```c
#include "../../src/light.c"
#include "prova.h"

static int HaUniform(Shader s, const char *nome)
{
    return GetShaderLocation(s, nome) != -1;
}

int main(void)
{
    /* ... apertura finestra nascosta come in Task 1 Step 2 ... */
    Ok("LightInit()", LightInit());
    if (!LightReady()) { CloseWindow(); return 1; }

    Shader inst = LightInstShader();
    Ok("il programma instanziato esiste", inst.id != 0);
    Ok("ed e' diverso da quello normale",  inst.id != gShader.id);

    /* Il fragment e' lo stesso file, quindi le stesse uniform devono
     * risolversi su tutti e due i programmi. */
    static const char *u[] = { "texture0", "texture2", "colDiffuse",
                               "lightDir", "sunAmount", "depthOnly",
                               "shadowOn", "shadowRes", "viewPos",
                               "splitDist", "lightVP0", "lightVP1",
                               "shadowMap0", "shadowMap1" };
    for (int i = 0; i < (int)(sizeof u / sizeof *u); i++) {
        char msg[96];
        snprintf(msg, sizeof msg, "instanziato: uniform %s", u[i]);
        Ok(msg, HaUniform(inst, u[i]));
    }

    /* Le due che il programma instanziato ha in piu': la mvp non esiste, perche'
     * la matrice del modello arriva per istanza. */
    Ok("instanziato: uniform matView",       HaUniform(inst, "matView"));
    Ok("instanziato: uniform matProjection", HaUniform(inst, "matProjection"));

    CloseWindow();
    return ProveEsito();
}
```

E la riga nel Makefile:

```make
	$(CC) $(PROVE_CF) tools/prove/luce.c      src/fmath.c                                           $(PROVE_LIB) -o $(PROVE_DIR)/luce
```

- [ ] **Step 2: eseguirla e vederla fallire**

Run: `make prove`
Expected: `luce` non compila — `LightInstShader` non esiste. È il fallimento
atteso.

- [ ] **Step 3: scrivere `assets/shaders/scene_inst.vs`**

```glsl
#version 330

/* Come scene.vs, ma la trasformazione arriva per ISTANZA invece che per
 * disegno. Il fragment shader e' lo stesso file: la luce vive in un posto solo.
 *
 * Gli slot 8 e 9 non sono scelti a caso: raylib usa 0-4 per posizione, UV,
 * normale, colore e tangente, 5 per le seconde UV, 6 per gli indici, 7 e 8 per
 * le ossa quando la skinning GPU e' accesa. Sopra l'8 e' terra libera, e
 * OpenGL 3.3 garantisce almeno sedici attributi. */
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexTangent;

/* xyz: posizione. w: seno e coseno dell'imbardata, calcolati dalla CPU una
 * volta per istanza invece che una volta per vertice. */
layout(location = 8) in vec4 instPosSin;
layout(location = 9) in vec4 instScaleCos;

uniform mat4 matView;
uniform mat4 matProjection;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragTangent;

/* Rotazione attorno a Y con lo stesso verso di MatrixRotateY() di raymath:
 * x' = cos*x + sin*z, z' = -sin*x + cos*z. Sbagliare il segno qui specchia
 * tutta la foresta, e non e' evidente guardando un albero solo. */
vec3 RuotaY(vec3 v, float s, float c)
{
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

void main()
{
    float s = instPosSin.w, c = instScaleCos.w;
    vec3  sc = instScaleCos.xyz;

    /* Stesso ordine di DrawModelEx: prima scala, poi rotazione, poi posizione. */
    vec3 world = RuotaY(vertexPosition * sc, s, c) + instPosSin.xyz;

    /* La normale si trasforma con l'inversa trasposta, che per scala e
     * rotazione attorno a Y vuol dire dividere per la scala e poi ruotare.
     * Dividere e non moltiplicare: con una scala non uniforme - la falda del
     * tetto, le primitive procedurali - moltiplicare darebbe normali storte e
     * l'illuminazione sbagliata proprio sugli oggetti schiacciati. */
    vec3 nrm = RuotaY(vertexNormal / sc, s, c);

    /* La tangente invece giace SULLA superficie, quindi segue la matrice del
     * modello come una posizione: si moltiplica per la scala. */
    vec3 tan = RuotaY(vertexTangent.xyz * sc, s, c);

    fragPosition = world;
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = normalize(nrm);
    fragTangent  = vec4(tan, vertexTangent.w);

    gl_Position = matProjection * matView * vec4(world, 1.0);
}
```

- [ ] **Step 4: far reggere due programmi a `light.c`**

In `light.c`, sostituire le variabili di location singole con insiemi paralleli:

```c
#define LIGHT_PROGRAMS 2      /* 0 = normale, 1 = instanziato */

static Shader gProg[LIGHT_PROGRAMS];
static int locLightDir[LIGHT_PROGRAMS], locSunAmount[LIGHT_PROGRAMS], ...;
static int locView[LIGHT_PROGRAMS], locProj[LIGHT_PROGRAMS];
```

`gShader` diventa un alias di `gProg[0]` per non riscrivere il resto del file:

```c
#define gShader gProg[0]
```

`LightInit()` carica il secondo programma **senza far fallire il primo** se
manca: se `scene_inst.vs` non c'è, `gProg[1].id` resta 0 e chi lo chiede riceve
uno shader nullo — il chiamante torna al percorso non instanziato.

```c
    /* Il secondo programma: stesso fragment, vertex diverso. Se manca, resta
     * a zero e i lotti d'istanza non si creano: si disegna come prima. */
    if (FileExists("assets/shaders/scene_inst.vs"))
        gProg[1] = LoadShader("assets/shaders/scene_inst.vs", "assets/shaders/scene.fs");
```

Ogni funzione che oggi chiama `SetShaderValue(gShader, locX, ...)` diventa un
ciclo su `LIGHT_PROGRAMS` che salta i programmi con `id == 0`. Riguarda
`LightFrame()`, `LightShadowBegin()` e `LightShadowEnd()`.

In `light.h`:

```c
/* Il programma per il disegno instanziato: stesso fragment, vertex che prende
 * la trasformazione per istanza. 'id' vale 0 se non e' stato caricato, e allora
 * chi voleva instanziare disegna come prima. */
Shader LightInstShader(void);
```

- [ ] **Step 5: eseguire la prova**

Run: `make prove`
Expected: `luce` stampa tutti `ok`. Le altre due continuano a passare.

- [ ] **Step 6: verificare che il gioco vero non sia cambiato**

Run: `make && timeout 10 ./frostmark 2>&1 | grep -Ei "warning|error|failed" | sort -u`
Expected: nessuna riga. Lo shader in più si carica, non cambia niente di ciò che
si vede.

- [ ] **Step 7: commit**

```bash
git add assets/shaders/scene_inst.vs src/light.c src/light.h tools/prove/luce.c Makefile
git commit -m "La luce serve due programmi: uno normale e uno instanziato

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 3: il modulo dei lotti

**Files:**
- Create: `src/instancing.h`, `src/instancing.c`
- Test: `tools/prove/instancing.c`
- Modify: `Makefile` (l'oggetto nuovo entra in `OBJS` da sé se usa il carattere
  jolly; verificarlo, e aggiungere la riga di prova)

**Interfaces:**
- Consumes: `LightInstShader()` (Task 2), `prova.h` (Task 1).
- Produces:
  ```c
  typedef struct InstBatch InstBatch;
  InstBatch *InstCreate(Mesh mesh, Material mat);
  void InstFree(InstBatch *b);
  void InstBegin(InstBatch *b);
  void InstAdd(InstBatch *b, Vector3 pos, float yawDeg, Vector3 scale);
  void InstFlush(InstBatch *b);
  int  InstCount(const InstBatch *b);
  ```
  `InstCreate` ritorna `NULL` se `LightInstShader().id == 0` o se la mesh non è
  caricata sulla GPU: il chiamante allora disegna come prima.

- [ ] **Step 1: scrivere la prova d'equivalenza, che deve fallire**

È la prova che conta: lo stesso oggetto disegnato nei due modi deve dare gli
stessi pixel.

```c
#include "../../src/instancing.c"
#include "prova.h"

/* Rende una scena in una texture e restituisce l'immagine. Camera, luce e
 * geometria sono identiche fra le due chiamate: cambia solo il percorso. */
static Image RendiConLotto(RenderTexture2D rt, InstBatch *b, Camera3D cam)
{
    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(cam);
            InstBegin(b);
            InstAdd(b, (Vector3){ 0, 0, 0 }, 35.0f, (Vector3){ 1.0f, 2.5f, 1.0f });
            InstFlush(b);
        EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(rt.texture);
}

static Image RendiNormale(RenderTexture2D rt, Mesh m, Material mat, Camera3D cam)
{
    Matrix t = MatrixMultiply(MatrixMultiply(
                   MatrixScale(1.0f, 2.5f, 1.0f),
                   MatrixRotate((Vector3){ 0, 1, 0 }, 35.0f * DEG2RAD)),
                   MatrixTranslate(0, 0, 0));
    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(cam);
            DrawMesh(m, mat, t);
        EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(rt.texture);
}

/* Quanti pixel differiscono di piu' della tolleranza. */
static int PixelDiversi(Image a, Image b, int tol)
{
    int n = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
            if (abs(ca.r - cb.r) > tol || abs(ca.g - cb.g) > tol ||
                abs(ca.b - cb.b) > tol) n++;
        }
    return n;
}
```

Il corpo di `main()`: finestra nascosta, `LightInit()`, una mesh con scala **non
uniforme** e rotazione **non nulla** — sono i due casi in cui un errore di segno
o di inversa trasposta si vede — poi:

```c
    Image ia = RendiConLotto(rt, lotto, cam);
    Image ib = RendiNormale(rt, mesh, mat, cam);
    int diversi = PixelDiversi(ia, ib, 2);
    printf("  pixel diversi: %d su %d\n", diversi, ia.width * ia.height);
    Ok("instanziato e non instanziato danno la stessa immagine", diversi == 0);

    /* E che non sia nero contro nero: se non ha disegnato niente, la prova
     * sopra passerebbe per il motivo sbagliato. */
    int accesi = 0;
    for (int y = 0; y < ib.height; y++)
        for (int x = 0; x < ib.width; x++)
            if (GetImageColor(ib, x, y).r > 8) accesi++;
    Ok("e qualcosa e' stato davvero disegnato", accesi > 200);
```

Usare `GenMeshCube(1,1,1)` come mesh: ha normali per faccia, quindi le facce
devono venire con luminosità **diverse fra loro** — il che prova che le normali
sono trasformate e non solo copiate.

- [ ] **Step 2: eseguirla e vederla fallire**

Run: `make prove`
Expected: non compila, `src/instancing.c` non esiste.

- [ ] **Step 3: scrivere `src/instancing.h`**

```c
/* ============================================================================
 * instancing.h - Disegnare la stessa mesh molte volte con una chiamata sola.
 *
 * Misurato prima di scrivere questo modulo: il passaggio principale costava
 * ~4,4 microsecondi per chiamata di disegno e quello d'ombra ~9, con mesh da
 * cento triangoli. Non erano i triangoli: erano le chiamate. Un lotto le
 * sostituisce tutte con una.
 *
 * Un lotto tiene un VAO PROPRIO, che punta ai VBO della mesh piu' il suo buffer
 * d'istanza. Non si tocca il VAO della mesh apposta: raylib, in
 * DrawMeshInstanced(), attacca gli attributi d'istanza al VAO della mesh e alla
 * fine cancella il buffer senza spegnere i divisor - e la stessa mesh, disegnata
 * poi senza instancing, leggerebbe un buffer che non esiste piu'. Qui i due
 * percorsi non si vedono nemmeno.
 * ========================================================================== */
#ifndef INSTANCING_H
#define INSTANCING_H

#include "raylib.h"

typedef struct InstBatch InstBatch;

/* Crea un lotto per una mesh e un materiale. Ritorna NULL se lo shader
 * instanziato non c'e' o la mesh non e' sulla GPU: il chiamante allora disegna
 * come prima, che e' il comportamento giusto quando assets/shaders/ manca. */
InstBatch *InstCreate(Mesh mesh, Material mat);
void       InstFree(InstBatch *b);

/* Un giro per passaggio: il passaggio principale culla a cono, quello d'ombra a
 * raggio, quindi le liste sono diverse e vanno rifatte. */
void InstBegin(InstBatch *b);
/* L'imbardata si passa in GRADI, come la porta il Prop e come la vuole
 * DrawModelEx. Seno e coseno si calcolano qui, una volta per istanza: e' il
 * motivo per cui il dato d'istanza ne porta due invece dell'angolo. */
void InstAdd(InstBatch *b, Vector3 pos, float yawDeg, Vector3 scale);
void InstFlush(InstBatch *b);

#endif /* INSTANCING_H */
```

- [ ] **Step 4: scrivere `src/instancing.c`**

Il dato d'istanza e la struttura:

```c
/* 32 byte. Seno e coseno arrivano gia' calcolati: farli nel vertex shader
 * significherebbe rifarli per ogni vertice di ogni istanza. */
typedef struct { float x, y, z, sinYaw; float sx, sy, sz, cosYaw; } InstData;

#define INST_LOC_POS    8      /* vedi il commento in scene_inst.vs */
#define INST_LOC_SCALE  9

struct InstBatch {
    Mesh     mesh;
    Material mat;
    unsigned int vao, ivbo;
    InstData *cpu;
    int count, cap;
    int elems;           /* indici, 0 se la mesh non e' indicizzata */
};
```

`InstCreate()` costruisce il VAO a mano, agganciando i VBO **della mesh** e poi
il buffer d'istanza:

```c
    /* Il materiale e' una COPIA: il chiamante continua a usare il suo, con lo
     * shader normale, per il percorso non instanziato. Qui dentro ci va il
     * secondo programma. */
    b->mat = mat;
    b->mat.shader = LightInstShader();
    if (b->mat.shader.id == 0) return NULL;      /* niente shader, niente lotto */

    b->elems = (mesh.indices != NULL) ? mesh.triangleCount * 3 : 0;

    b->vao = rlLoadVertexArray();
    rlEnableVertexArray(b->vao);

    rlEnableVertexBuffer(mesh.vboId[0]);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);

    rlEnableVertexBuffer(mesh.vboId[1]);
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(1);

    rlEnableVertexBuffer(mesh.vboId[2]);
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(2);

    /* Colore e tangente possono mancare: si da' un valore fisso, come fa
     * UploadMesh(). Bianco per il colore, zero per la tangente - e il fragment
     * shader sa gia' che una tangente nulla vuol dire "usa la normale". */
    if (mesh.vboId[3] != 0) {
        rlEnableVertexBuffer(mesh.vboId[3]);
        rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlEnableVertexAttribute(3);
    } else {
        float bianco[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        rlSetVertexAttributeDefault(3, bianco, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(3);
    }

    if (mesh.vboId[4] != 0) {
        rlEnableVertexBuffer(mesh.vboId[4]);
        rlSetVertexAttribute(4, 4, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(4);
    } else {
        float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(4, zero, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(4);
    }

    if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[6]);

    /* Il buffer d'istanza: dinamico, e non viene mai distrutto per fotogramma. */
    b->cap  = 256;
    b->ivbo = rlLoadVertexBuffer(NULL, b->cap * (int)sizeof(InstData), true);
    rlEnableVertexBuffer(b->ivbo);
    rlSetVertexAttribute(INST_LOC_POS, 4, RL_FLOAT, false, sizeof(InstData), 0);
    rlSetVertexAttributeDivisor(INST_LOC_POS, 1);
    rlEnableVertexAttribute(INST_LOC_POS);
    rlSetVertexAttribute(INST_LOC_SCALE, 4, RL_FLOAT, false, sizeof(InstData), 16);
    rlSetVertexAttributeDivisor(INST_LOC_SCALE, 1);
    rlEnableVertexAttribute(INST_LOC_SCALE);

    rlDisableVertexArray();
```

`InstAdd()` cresce per raddoppi. Quando il buffer CPU cresce oltre la capacità
GPU, il VBO va ricreato **e riagganciato al VAO**, perché l'aggancio è per id:

```c
static void Cresci(InstBatch *b, int voluto)
{
    while (b->cap < voluto) b->cap *= 2;
    b->cpu = (InstData *)MemRealloc(b->cpu, (unsigned int)(b->cap * (int)sizeof(InstData)));

    rlUnloadVertexBuffer(b->ivbo);
    b->ivbo = rlLoadVertexBuffer(NULL, b->cap * (int)sizeof(InstData), true);

    rlEnableVertexArray(b->vao);
    rlEnableVertexBuffer(b->ivbo);
    rlSetVertexAttribute(INST_LOC_POS, 4, RL_FLOAT, false, sizeof(InstData), 0);
    rlSetVertexAttributeDivisor(INST_LOC_POS, 1);
    rlEnableVertexAttribute(INST_LOC_POS);
    rlSetVertexAttribute(INST_LOC_SCALE, 4, RL_FLOAT, false, sizeof(InstData), 16);
    rlSetVertexAttributeDivisor(INST_LOC_SCALE, 1);
    rlEnableVertexAttribute(INST_LOC_SCALE);
    rlDisableVertexArray();
}
```

`InstFlush()` carica, imposta le uniform che raylib per noi non imposta più
(`matView`, `matProjection`, `colDiffuse` e le texture dei materiali), disegna,
e rimette le cose com'erano:

```c
void InstFlush(InstBatch *b)
{
    if (b == NULL || b->count == 0) return;

    rlUpdateVertexBuffer(b->ivbo, b->cpu, b->count * (int)sizeof(InstData), 0);
    rlEnableShader(b->mat.shader.id);

    /* Dentro BeginMode3D la modelview E' la vista: non c'e' matrice di modello,
     * quella arriva per istanza. Vale anche nel passaggio d'ombra, dove
     * BeginMode3D ha ricevuto la camera del sole. */
    Matrix vista = rlGetMatrixModelview();
    Matrix proj  = rlGetMatrixProjection();
    if (b->mat.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1)
        rlSetUniformMatrix(b->mat.shader.locs[SHADER_LOC_MATRIX_VIEW], vista);
    if (b->mat.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1)
        rlSetUniformMatrix(b->mat.shader.locs[SHADER_LOC_MATRIX_PROJECTION], proj);

    if (b->mat.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1) {
        float col[4] = { b->mat.maps[MATERIAL_MAP_DIFFUSE].color.r / 255.0f,
                         b->mat.maps[MATERIAL_MAP_DIFFUSE].color.g / 255.0f,
                         b->mat.maps[MATERIAL_MAP_DIFFUSE].color.b / 255.0f,
                         b->mat.maps[MATERIAL_MAP_DIFFUSE].color.a / 255.0f };
        rlSetUniform(b->mat.shader.locs[SHADER_LOC_COLOR_DIFFUSE], col, SHADER_UNIFORM_VEC4, 1);
    }

    for (int i = 0; i < MAX_MATERIAL_MAPS; i++) {
        if (b->mat.maps[i].texture.id == 0) continue;
        rlActiveTextureSlot(i);
        rlEnableTexture(b->mat.maps[i].texture.id);
        rlSetUniform(b->mat.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
    }

    rlEnableVertexArray(b->vao);
    if (b->elems > 0) rlDrawVertexArrayElementsInstanced(0, b->elems, 0, b->count);
    else              rlDrawVertexArrayInstanced(0, b->mesh.vertexCount, b->count);
    rlDisableVertexArray();

    for (int i = 0; i < MAX_MATERIAL_MAPS; i++) {
        if (b->mat.maps[i].texture.id == 0) continue;
        rlActiveTextureSlot(i);
        rlDisableTexture();
    }
    rlDisableShader();
}
```

`InstFree()` scarica VAO e VBO e libera l'array CPU. **Non** scarica la mesh né
il materiale: il lotto li usa, non li possiede.

- [ ] **Step 5: eseguire la prova**

Run: `make prove`
Expected: `pixel diversi: 0`, entrambi i controlli `ok`.

Se i pixel diversi sono molti e l'immagine instanziata è **specchiata**, il segno
della rotazione in `RuotaY()` è invertito. Se le facce hanno la luminosità
sbagliata ma la forma è giusta, è la normale: si sta moltiplicando per la scala
invece di dividere.

- [ ] **Step 6: commit**

```bash
git add src/instancing.c src/instancing.h tools/prove/instancing.c Makefile
git commit -m "Un lotto disegna la stessa mesh molte volte in una chiamata

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 4: prop esterni instanziati (tappa 1) e misura

**Files:**
- Modify: `src/world.h` (campo lotti in `World`), `src/world.c`
  (`LoadExtProps()`, `WorldUnload()`, `WorldDrawProps()`)

**Interfaces:**
- Consumes: tutta l'API di `instancing.h` (Task 3).
- Produces: `World` guadagna `InstBatch *propBatch[PROP_COUNT]`, `NULL` dove il
  tipo non ha modello esterno o lo shader instanziato manca.

- [ ] **Step 1: creare i lotti dove si caricano i modelli**

In `LoadExtProps()`, dopo `LightApplyToModel(&m)`, per ogni tipo caricato:

```c
        /* Un lotto per la prima mesh del modello. I prop del kit hanno una
         * mesh sola; se ne avessero piu' d'una servirebbe un lotto per mesh, e
         * qui si torna al disegno normale invece di disegnarne meta'. */
        if (m.meshCount == 1)
            w->propBatch[t] = InstCreate(m.meshes[0], m.materials[0]);
```

In `WorldUnload()`, prima di `UnloadModel()`:

```c
        if (w->propBatch[t] != NULL) { InstFree(w->propBatch[t]); w->propBatch[t] = NULL; }
```

- [ ] **Step 2: riempire i lotti invece di disegnare**

In `WorldDrawProps()`, il ciclo di culling non cambia. Cambia cosa fa in fondo:

```c
    /* Svuota le liste: il passaggio principale culla a cono, quindi la lista e'
     * diversa da quella del passaggio d'ombra. */
    for (int t = 0; t < PROP_COUNT; t++)
        if (w->propBatch[t] != NULL) InstBegin(w->propBatch[t]);

    /* ... il ciclo esistente, con al posto di DrawProp(): ... */
            InstBatch *b = w->propBatch[p->type];
            if (b != NULL && !p->taken) {
                float k = p->scale * gExtProp[p->type].scale;
                InstAdd(b, p->pos, p->rot, (Vector3){ k, k, k });
            } else {
                DrawProp(w, p, tint, d2 > lodD2);    /* primitive procedurali */
            }

    /* ... e dopo il ciclo: ... */
    for (int t = 0; t < PROP_COUNT; t++)
        if (w->propBatch[t] != NULL) InstFlush(w->propBatch[t]);
```

Nota: la tinta del ciclo giorno/notte per i modelli esterni è
`Shade(WHITE, tint)`, che vale `tint`. Va messa in
`b->mat.maps[MATERIAL_MAP_DIFFUSE].color` prima di `InstFlush()`, una volta per
lotto, non per istanza.

- [ ] **Step 3: verificare che non sia cambiato niente a vedersi**

Run: `make && make prove && timeout 10 ./frostmark 2>&1 | grep -Ei "warning|error" | sort -u`
Expected: prove passate, nessun avviso. Gli alberi si vedono come prima: se
sono spariti, il lotto è `NULL`; se sono neri, manca la texture; se sono nel
posto sbagliato, è l'ordine scala/rotazione/posizione.

- [ ] **Step 4: misurare**

Ricostruire il binario strumentato (copia dei sorgenti fuori dal repo, forzare
`GS_PLAY` in `GameInit()` **e** in `GameNewWorld()`, far ruotare
`g->player.yaw`) e girare 75 secondi.

Confronto con la baseline:

| | baseline | atteso dopo |
|---|---|---|
| chiamate, caso peggiore | 1.766 | ~500 (restano ombre ed edifici) |
| passaggio principale, peggiore | 6,0 ms | < 3 ms |

Se le chiamate non scendono, i lotti non si stanno creando: controllare che
`LightInstShader().id` non sia 0.

- [ ] **Step 5: commit con i numeri nel messaggio**

`X`, `Y` e `Z` non sono segnaposto da lasciare: sono i numeri appena misurati
allo Step 4, e vanno sostituiti prima di committare.

```bash
git add src/world.c src/world.h
git commit -m "I prop esterni si disegnano a lotti, non uno per volta

Il passaggio principale passa da X a Y ms nel caso peggiore, le chiamate
da 1766 a Z. Misurato sullo stesso percorso di 75 s della baseline.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 5: pezzi d'edificio (tappa 2) e misura

**Files:**
- Modify: `src/world.h`, `src/world.c` (`LoadBuildParts()`, `PlacePart()`,
  `DrawHouse()`, `DrawTower()`, `WorldUnload()`)

**Interfaces:**
- Consumes: `instancing.h`.
- Produces: `World` guadagna `InstBatch *partBatch[BUILD_PART_COUNT]`.

Una casa bassa costa 19 chiamate, una alta 45, una torre 5. Sono dieci tipi di
pezzo, quindi dieci lotti per tutti gli edifici in vista.

- [ ] **Step 1: creare i lotti in `LoadBuildParts()`, liberarli in `WorldUnload()`**

In `LoadBuildParts()`, dopo che il pezzo e' stato caricato e ha ricevuto
`LightApplyToModel()`:

```c
        /* I pezzi del kit hanno una mesh sola. Se ne avessero piu' d'una, il
         * lotto non si crea e si torna al disegno normale: meglio lento che
         * meta' edificio. */
        if (w->buildPart[i].meshCount == 1)
            w->partBatch[i] = InstCreate(w->buildPart[i].meshes[0],
                                         w->buildPart[i].materials[0]);
```

In `WorldUnload()`, prima di `UnloadModel(w->buildPart[i])`:

```c
        if (w->partBatch[i] != NULL) { InstFree(w->partBatch[i]); w->partBatch[i] = NULL; }
```

E in `world.h`, dentro `World`:

```c
    InstBatch *partBatch[BUILD_PART_COUNT];
```

- [ ] **Step 2: far accodare `PlacePart()` invece di disegnare**

`PlacePart()` calcola già posizione e rotazione finali. Il corpo diventa:

```c
    InstBatch *b = w->partBatch[part];
    if (b != NULL) InstAdd(b, p, rotDeg + localRot, scale);
    else DrawModelEx(w->buildPart[part], p, (Vector3){ 0.0f, 1.0f, 0.0f },
                     rotDeg + localRot, scale, tint);
```

- [ ] **Step 3: gestire il tetto, che spegne lo scarto delle facce posteriori**

`DrawHouse()` chiama `rlDisableBackfaceCulling()` attorno ai pezzi del tetto. Con
i lotti il disegno non avviene più lì ma allo svuotamento, quindi la chiamata
attorno al ciclo **non ha più effetto**. Il tetto va svuotato a parte:

```c
    /* Il tetto e' un guscio sottile: da dentro casa se ne vedrebbe attraverso.
     * Va svuotato da solo, con lo scarto delle facce posteriori spento - non
     * basta piu' spegnerlo attorno al ciclo che accoda, perche' il disegno
     * avviene dopo. */
    rlDisableBackfaceCulling();
    InstFlush(w->partBatch[BUILD_ROOF]);
    InstBegin(w->partBatch[BUILD_ROOF]);
    rlEnableBackfaceCulling();
```

È il tranello di questo compito: dimenticarlo dà soffitti trasparenti dentro
casa, e non si vede da fuori.

- [ ] **Step 4: verificare a occhio e con le prove**

Run: `make && make prove && timeout 10 ./frostmark 2>&1 | grep -Ei "warning|error" | sort -u`
Expected: nessun avviso. Entrare in una casa e guardare in su: il soffitto c'è.

- [ ] **Step 5: misurare come nel Task 4 Step 4, e commit**

```bash
git add src/world.c src/world.h
git commit -m "Anche i pezzi degli edifici vanno a lotti

Una casa alta costava 45 chiamate, ora zero: entra nei dieci lotti dei
pezzi. Da X a Y ms nel caso peggiore.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 6: passaggio d'ombra (tappa 3) e misura

**Files:**
- Modify: `src/world.c` (`WorldDrawShadowCasters()`)

È il costo fisso più grande: 3,7 ms per ~400 chiamate, e **non** è riempimento —
misurato abbassando `SHADOW_RES` da 2048 a 1024 senza che il tempo cambiasse.

- [ ] **Step 1: usare gli stessi lotti nel passaggio d'ombra**

`WorldDrawShadowCasters()` prende la stessa forma del Task 4 Step 2:
`InstBegin()` su tutti i lotti, il ciclo esistente che accoda, `InstFlush()` alla
fine. Il culling resta a raggio, non a cono.

Il passaggio d'ombra viene chiamato **una volta per cascata**, quindi
`Begin`/`Flush` avvengono due volte per fotogramma con liste diverse: è
esattamente il motivo per cui l'API è a tre tempi invece che a uno.

- [ ] **Step 2: verificare che le ombre non siano cambiate**

Run: `make && timeout 10 ./frostmark`
Expected: le ombre degli alberi ci sono ancora e cadono dove cadevano. Se sono
sparite, i lotti si svuotano fuori dal `BeginTextureMode()` della cascata.

- [ ] **Step 3: misurare**

| | baseline | atteso dopo |
|---|---|---|
| passaggio ombre | 3,7 ms | < 1,5 ms |
| chiamate d'ombra | ~400 | ~15 |

- [ ] **Step 4: commit**

```bash
git add src/world.c
git commit -m "Il passaggio d'ombra usa gli stessi lotti

Erano 3,7 ms fissi per 400 chiamate a 9 microsecondi l'una. Non era
riempimento: quattro volte meno texel non cambiava il tempo.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 7: documentazione

**Files:**
- Modify: `docs/01-architettura.md`

- [ ] **Step 1: aggiungere una sezione «Instancing» dopo «Normal map»**

Deve contenere, perché sono le cose che non si ricostruiscono a memoria:

- La misura che ha deciso il piano: ~4,4 µs per chiamata nel passaggio
  principale, ~9 nel passaggio d'ombra, e la prova che le ombre **non** sono
  limitate dal riempimento (2048 → 1024 non cambia niente).
- Perché il VAO è nostro e non quello della mesh: il divisor che raylib non
  azzera.
- Perché il dato d'istanza è di 32 byte e non 100: in questo gioco non esistono
  rotazioni libere.
- Perché la normale si **divide** per la scala mentre la tangente si moltiplica.
- Il tranello del tetto (Task 5 Step 3).
- I numeri prima e dopo, per ogni tappa.

- [ ] **Step 2: commit**

```bash
git add docs/01-architettura.md
git commit -m "Documentato l'instancing, con i numeri che l'hanno deciso

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Cosa NON è in questo piano

**Gli impostori** (tappa 4 dello spec). Sono un sottosistema a sé — rendering
fuori schermo all'avvio, shader del billboard, ritaglio alfa nel passaggio di
profondità — e lo spec dice che ogni tappa si misura prima della successiva.
Meritano un piano proprio, scritto **dopo** aver visto i numeri del Task 6: se
l'instancing da solo porta il caso peggiore sotto i 6 ms, la distanza di vista si
può alzare senza impostori, e il piano cambia forma.

Fuori anche, come già detto nello spec: culling a frustum, terreno, personaggi,
acqua, UI, decimazione delle mesh.
