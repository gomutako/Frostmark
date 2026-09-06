# Materiali proiettati sugli edifici — piano d'implementazione

> **Per chi esegue:** SOTTO-SKILL RICHIESTA — usare superpowers:subagent-driven-development (consigliata) o superpowers:executing-plans, un compito per volta. I passi hanno la casella `- [ ]` per tenere il conto.

**Obiettivo:** dare materiali fotogrammetrici ai dieci pezzi modulari degli edifici, che non hanno UV utilizzabili, ricavando la coordinata di texture dalla posizione invece che dalle UV.

**Architettura:** il fragment shader guadagna tre modi — UV come oggi, proiezione sull'asse dominante, miscela a tre proiezioni — scelti da un uniform che viaggia **per lotto**, come già fa `alphaCut`. La proiezione avviene in **spazio oggetto**, non di mondo, perché una casa ruotata prenderebbe altrimenti la venatura di traverso; i due vertex shader passano al fragment la posizione locale già moltiplicata per la scala, la normale locale, e quel che serve a riportare in mondo la normale perturbata.

**Tecnologie:** C99, raylib 5.5 (vendored in `vendor/raylib`), GLSL 330 via `rlgl`. Nessuna dipendenza nuova.

**Spec:** `docs/superpowers/specs/2026-09-06-materiali-triplanari-design.md`

## Vincoli globali

- **I sorgenti sono in ASCII.** Commenti in italiano ma senza lettere accentate: `cosi'`, `piu'`, `e'`. Vale per `src/`, `tools/`, `assets/shaders/` e i messaggi di commit. I documenti in `docs/` usano gli accenti veri.
- **Zero avvisi.** `-std=gnu99 -Wall -Wextra`; `make` costruisce Linux **e** Windows (sotto WSL) e non deve stampare avvisi.
- **Un uniform che nessuno usa sparisce.** Il compilatore GLSL elimina gli uniform non referenziati e `GetShaderLocation()` torna -1. Per questo il Task 1 introduce l'uniform **e** il codice che lo usa: non si possono separare.
- Le prove includono il `.c` che provano (`#include "../../src/light.c"`); un modulo incluso non va anche collegato o i simboli si duplicano.
- Una prova senza contesto OpenGL esce `PROVA_SALTATA` (77) e `make prove` la conta come saltata.
- **Ogni prova nuova va verificata sabotando il codice**: si rompe di proposito la funzione provata, si controlla che la prova fallisca, si rimette a posto.
- Il gioco deve continuare a funzionare **senza** `assets/` : texture mancanti significano modo 0 e la tavolozza del kit come oggi, non un errore.
- Il mondo cotto non va rigenerato. `BUILD_CELL` e la geometria dei pezzi non si toccano.

---

### Task 1: Il canale e l'asse dominante

**File:**
- Modifica: `assets/shaders/scene.vs`, `assets/shaders/scene_inst.vs`, `assets/shaders/scene.fs`
- Modifica: `src/light.h`, `src/light.c`, `src/instancing.h`, `src/instancing.c`
- Crea: `tools/prove/proiezione.c`
- Modifica: `Makefile` (ricetta `prove`)

**Interfacce:**
- Consuma: `InstCreate()`, `InstFlush()`, `LightApplyToMaterial()`, già esistenti.
- Produce:
  - uniform `int projMode` (0 = UV, 1 = asse dominante, 2 = miscela) e `float projTile` (metri per ripetizione) in `scene.fs`;
  - nella prova, `static bool SulQuadrato(Color c)` — vero se il pixel sta sul quadrato e non sullo sfondo — che il Task 2 riusa;
  - `void InstProjection(InstBatch *b, int mode, float tile);`
  - `void InstModelProjection(InstModel *im, int mode, float tile);`
  - `void LightSetProjection(int mode, float tile);` per il percorso non instanziato.

- [ ] **Passo 1: scrivere la prova che fallisce**

Crea `tools/prove/proiezione.c`:

```c
/* ============================================================================
 * proiezione.c - La texture ricavata dalla posizione invece che dalle UV.
 *
 * I pezzi dei kit Kenney non hanno UV utilizzabili: il muro ha 64 vertici e
 * tutte le sue coordinate stanno in una cella della tavolozza. Per loro la
 * texture si proietta, e questa prova fissa le due proprieta' che rendono la
 * proiezione utile invece che dannosa:
 *
 *   1. l'asse scelto e' quello della faccia;
 *   2. la proiezione e' in SPAZIO OGGETTO, cioe' il motivo e' incollato alla
 *      casa e ci gira insieme. Proiettando in coordinate di mondo la casa
 *      ruotata prende la venatura di traverso, e la differenza si vede solo
 *      confrontando due istanze ruotate diversamente.
 *
 * Il trucco della prova: la texture e' una RAMPA orizzontale, quindi il canale
 * rosso di un pixel DICE la coordinata U che quel frammento ha campionato. Si
 * legge un numero invece di indovinare un colore.
 * ========================================================================== */
#include "../../src/light.c"
#include "../../src/instancing.c"
#include "prova.h"

#include <string.h>
#include <unistd.h>

#define RT 120

/* Quadrato di 4x4 m nel piano XZ, normale +Y. Le UV coprono 0..1 sul quadrato:
 * servono al modo 0, quello che non deve cambiare. */
static Mesh QuadratoXZ(void)
{
    static float v[18]  = { -2,0,-2,  -2,0,2,   2,0,2,
                            -2,0,-2,   2,0,2,   2,0,-2 };
    static float n[18]  = { 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0 };
    static float uv[12] = { 0,0, 0,1, 1,1, 0,0, 1,1, 1,0 };

    Mesh m = { 0 };
    m.vertexCount = 6;
    m.triangleCount = 2;
    m.vertices = v; m.normals = n; m.texcoords = uv;
    UploadMesh(&m, false);
    return m;
}

/* Rampa orizzontale: il rosso cresce da 0 a 255 lungo U, il resto e' fisso.
 * Con il filtro lineare il valore letto e' la U campionata, a meno di un
 * texel. */
static Texture2D Rampa(void)
{
    Image im = GenImageColor(256, 4, BLACK);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 256; x++)
            ImageDrawPixel(&im, x, y, (Color){ (unsigned char)x, 40, 40, 255 });
    Texture2D t = LoadTextureFromImage(im);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    UnloadImage(im);
    return t;
}

/* Disegna il quadrato con l'imbardata data e torna l'immagine. La camera
 * guarda dall'alto con 'up' su +Z: lo schermo x segue il mondo x, lo schermo y
 * segue il mondo z. */
static Image Rendi(RenderTexture2D rt, InstBatch *b, float yawDeg)
{
    Camera3D cam = { 0 };
    cam.position   = (Vector3){ 0.0f, 5.0f, 0.0f };
    cam.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up         = (Vector3){ 0.0f, 0.0f, 1.0f };
    cam.fovy       = 4.2f;
    cam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(cam);
            rlDisableBackfaceCulling();
            InstBegin(b);
            InstAdd(b, (Vector3){ 0, 0, 0 }, yawDeg, (Vector3){ 1, 1, 1 });
            InstFlush(b);
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    return LoadImageFromTexture(rt.texture);
}

/* Quante volte il rosso CROLLA fra due pixel vicini lungo una riga: e' il
 * numero di ripetizioni della rampa meno una. Dice quante volte la texture si
 * ripete sul quadrato, che e' cio' che distingue un passo dall'altro. */
static int SaltiInRiga(Image im, int y)
{
    int n = 0;
    for (int x = 1; x < im.width; x++) {
        int a = GetImageColor(im, x - 1, y).r, b = GetImageColor(im, x, y).r;
        if (a - b > 100) n++;
    }
    return n;
}

static int SaltiInColonna(Image im, int x)
{
    int n = 0;
    for (int y = 1; y < im.height; y++) {
        int a = GetImageColor(im, x, y - 1).r, b = GetImageColor(im, x, y).r;
        if (a - b > 100) n++;
    }
    return n;
}

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova proiezione");
    if (!IsWindowReady()) {
        printf("niente contesto GL: prova saltata\n");
        return PROVA_SALTATA;
    }

    Ok("LightInit()", LightInit());
    if (!LightReady()) { CloseWindow(); return 1; }

    LightSetSun((Vector3){ 0.0f, 1.0f, 0.0f }, 1.0f);
    Camera3D nulla = { 0 };
    LightFrame(nulla);
    int zero = 0;
    for (int p = 0; p < PROG_COUNT; p++)
        if (gProg[p].id != 0)
            SetShaderValue(gProg[p], GetShaderLocation(gProg[p], "shadowOn"),
                           &zero, SHADER_UNIFORM_INT);

    RenderTexture2D rt = LoadRenderTexture(RT, RT);
    Mesh q = QuadratoXZ();

    Material mat = LoadMaterialDefault();
    LightApplyToMaterial(&mat);
    mat.maps[MATERIAL_MAP_DIFFUSE].texture = Rampa();

    InstBatch *b = InstCreate(q, mat);
    Ok("lotto creato", b != NULL);
    if (b == NULL) { CloseWindow(); return 1; }

    /* --- 1. modo 0: le UV della mesh, una rampa sola sul quadrato --------- */
    InstProjection(b, 0, 1.0f);
    Image im0 = Rendi(rt, b, 0.0f);
    int salti0 = SaltiInRiga(im0, RT / 2);
    Ok("modo 0: la mesh usa le sue UV, una rampa sola", salti0 == 0);

    /* --- 2. modo 1: proiezione con passo di 1 m sul quadrato da 4 m ------ */
    InstProjection(b, 1, 1.0f);
    Image im1 = Rendi(rt, b, 0.0f);
    int salti1 = SaltiInRiga(im1, RT / 2);
    printf("  salti in riga: modo 0 = %d, modo 1 = %d\n", salti0, salti1);
    Ok("modo 1: il passo di 1 m ripete la texture sul quadrato di 4 m",
       salti1 >= 3);

    /* Il quadrato ha normale +Y, quindi l'asse dominante e' Y e la U segue
     * la x del mondo: lungo la z - le colonne dell'immagine - non deve
     * cambiare niente. */
    Ok("modo 1: sulla faccia +Y la U segue x e non z",
       SaltiInColonna(im1, RT / 2) == 0);

    /* --- 3. la proiezione e' in spazio oggetto --------------------------- */
    /* Ruotando l'istanza di 90 gradi il motivo deve girare CON l'oggetto: i
     * salti passano dalle righe alle colonne. Proiettando in coordinate di
     * mondo resterebbero nelle righe, perche' la texture scivolerebbe sotto
     * l'oggetto invece di essere incollata. */
    Image im90 = Rendi(rt, b, 90.0f);
    int saltiRiga90 = SaltiInRiga(im90, RT / 2);
    int saltiCol90  = SaltiInColonna(im90, RT / 2);
    printf("  ruotato di 90 gradi: salti in riga %d, in colonna %d\n",
           saltiRiga90, saltiCol90);
    Ok("spazio oggetto: a 90 gradi il motivo gira con l'oggetto",
       saltiCol90 >= 3 && saltiRiga90 == 0);

    /* --- 4. due istanze in posizioni diverse ricevono fasi diverse ------- */
    /* Senza sfalsamento, trenta case avrebbero la venatura identica nello
     * stesso punto del proprio corpo. Si confronta il centro del QUADRATO, non
     * il centro dello schermo: spostando l'istanza di mezzo passo, il suo
     * centro si sposta anche sullo schermo, e la camera ortografica rende il
     * conto esatto - fovy 4.2 su RT pixel fa RT/4.2 pixel per metro. */
    Image imP = Rendi(rt, b, 0.0f);              /* istanza all'origine */
    int centro = RT / 2;
    int rossoOrigine = GetImageColor(imP, centro, centro).r;

    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(((Camera3D){ .position = { 0.0f, 5.0f, 0.0f },
                                 .target = { 0.0f, 0.0f, 0.0f },
                                 .up = { 0.0f, 0.0f, 1.0f },
                                 .fovy = 4.2f,
                                 .projection = CAMERA_ORTHOGRAPHIC }));
            rlDisableBackfaceCulling();
            InstBegin(b);
            InstAdd(b, (Vector3){ 0.5f, 0, 0 }, 0.0f, (Vector3){ 1, 1, 1 });
            InstFlush(b);
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();
    Image imQ = LoadImageFromTexture(rt.texture);

    int spostamento = (int)(0.5f * (float)RT / 4.2f);   /* mezzo metro in pixel */
    int rossoSpostato = GetImageColor(imQ, centro + spostamento, centro).r;

    printf("  rosso al centro del corpo: all'origine %d, spostata %d\n",
           rossoOrigine, rossoSpostato);
    Ok("lo sfalsamento cambia la fase fra istanze in posizioni diverse",
       abs(rossoOrigine - rossoSpostato) > 60);

    UnloadImage(im0); UnloadImage(im1); UnloadImage(im90);
    UnloadImage(imP); UnloadImage(imQ);
    InstFree(b);
    UnloadRenderTexture(rt);
    CloseWindow();
    return ProveEsito();
}
```

Aggiungi la riga nella ricetta `prove` del `Makefile`, subito dopo quella di `alfa`. Collega gli stessi moduli di `alfa` — `light.c` e `instancing.c` sono **inclusi**, non collegati:

```make
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/proiezione.c \
	      $(SRC_DIR)/fmath.c $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/proiezione
```

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: errore di compilazione, `InstProjection` non dichiarata.

- [ ] **Passo 3: i due vertex shader passano lo spazio oggetto**

In `assets/shaders/scene.vs`, aggiungi accanto agli altri `out`:

```glsl
/* Per i materiali proiettati: la posizione e la normale in spazio OGGETTO, e
 * quel che serve al fragment per riportare in mondo la normale perturbata.
 * La posizione e' gia' moltiplicata per la scala, cioe' e' in metri: senza,
 * la texture si stirerebbe con il pezzo, e la falda del tetto e' scalata
 * (cella, cella*1.6, cella*nz). */
out vec3 fragLocal;
out vec3 fragLocalNormal;
out vec2 fragYawSC;       /* seno e coseno dell'imbardata */
out vec3 fragInvScale;
out vec2 fragProjOffset;  /* sfalsamento per istanza, in metri */
```

e in fondo a `main()`, prima di `gl_Position`:

```glsl
    /* Scala e imbardata stanno dentro matModel: la scala e' la lunghezza delle
     * prime tre colonne, e la prima colonna normalizzata e' l'asse X ruotato,
     * cioe' (cos, 0, -sin) con la convenzione di RuotaY(). */
    vec3 sc = vec3(length(matModel[0].xyz), length(matModel[1].xyz),
                   length(matModel[2].xyz));
    vec3 ax = matModel[0].xyz / max(sc.x, 1e-6);
    fragLocal       = vertexPosition * sc;
    fragLocalNormal = vertexNormal;
    fragYawSC       = vec2(-ax.z, ax.x);
    fragInvScale    = 1.0 / max(sc, vec3(1e-6));
    fragProjOffset  = vec2(matModel[3].x, matModel[3].z);
```

In `assets/shaders/scene_inst.vs` aggiungi gli stessi cinque `out` con lo stesso commento, e in `main()`, dopo il calcolo di `tan`:

```glsl
    /* Qui scala e imbardata arrivano dal dato d'istanza: niente da estrarre. */
    fragLocal       = vertexPosition * sc;
    fragLocalNormal = vertexNormal;
    fragYawSC       = vec2(s, c);
    fragInvScale    = 1.0 / max(sc, vec3(1e-6));
    fragProjOffset  = vec2(instPosSin.x, instPosSin.z);
```

- [ ] **Passo 4: il fragment sceglie l'asse**

In `assets/shaders/scene.fs`, accanto agli altri `in` e `uniform`:

```glsl
in vec3 fragLocal;
in vec3 fragLocalNormal;
in vec2 fragYawSC;
in vec3 fragInvScale;
in vec2 fragProjOffset;

/* 0 = le UV della mesh, 1 = proiezione sull'asse dominante, 2 = miscela a tre.
 * Viaggia per LOTTO, come alphaCut. */
uniform int   projMode;
uniform float projTile;    /* metri coperti da una ripetizione */
```

e sopra `SurfaceNormal()`:

```glsl
/* --- Materiali proiettati --------------------------------------------------
 * I pezzi dei kit non hanno UV utilizzabili: il muro ha 64 vertici e tutte le
 * sue coordinate stanno in una cella della tavolozza. Per loro la texture si
 * ricava dalla POSIZIONE, e in spazio oggetto: proiettando in coordinate di
 * mondo, una casa ruotata prenderebbe la venatura di traverso.
 *
 * Asse dominante e non miscela a tre: i pezzi sono pannelli allineati agli
 * assi, quindi su una faccia piatta la scelta e' netta e costa un prelievo
 * invece di tre. La miscela serve solo dove la normale e' diagonale. */
int AsseDominante(vec3 n)
{
    vec3 a = abs(n);
    if (a.x >= a.y && a.x >= a.z) return 1;
    if (a.z >= a.y) return 2;
    return 0;
}

/* La V segue sempre la verticale dell'oggetto sulle facce laterali: e' quello
 * che tiene le assi verticali e le file parallele al muro. */
vec2 ProiettaUV(vec3 p, int asse)
{
    if (asse == 1) return vec2(p.z, p.y);
    if (asse == 2) return vec2(p.x, p.y);
    return vec2(p.x, p.z);
}

vec2 CoordProiettata()
{
    vec2 uv = ProiettaUV(fragLocal, AsseDominante(fragLocalNormal));
    /* Lo sfalsamento e' una TRASLAZIONE, non una rotazione: ruotare romperebbe
     * la verticalita' delle assi, che e' la ragione dello spazio oggetto.
     * Senza, trenta case avrebbero la venatura identica nello stesso punto. */
    return (uv + fragProjOffset) / max(projTile, 1e-4);
}
```

In `main()`, sostituisci la prima riga:

```glsl
    vec2 uv = (projMode == 0) ? fragTexCoord : CoordProiettata();
    vec4 albedo = texture(texture0, uv) * colDiffuse * fragColor;
```

Il rilievo continua per ora a leggere `fragTexCoord`: la normal map sotto proiezione e' il Task 3, e fino ad allora i materiali proiettati restano piatti. Non si vede, perche' gli edifici si accendono nel Task 5.

- [ ] **Passo 5: il dato per lotto in `src/instancing.c`**

In `struct InstBatch`, accanto ai campi dell'alfa:

```c
    int   locProjMode, locProjTile;   /* -1 se lo shader non li ha */
    int   projMode;                   /* 0 = UV, come prima        */
    float projTile;                   /* metri per ripetizione     */
```

In `InstCreate()`, accanto a `b->locAlphaCut`:

```c
    b->locProjMode = GetShaderLocation(sh, "projMode");
    b->locProjTile = GetShaderLocation(sh, "projTile");
    b->projMode    = 0;
    b->projTile    = 1.0f;
```

In `InstFlush()`, accanto a dove si carica `alphaCut`:

```c
    if (b->locProjMode != -1)
        rlSetUniform(b->locProjMode, &b->projMode, SHADER_UNIFORM_INT, 1);
    if (b->locProjTile != -1)
        rlSetUniform(b->locProjTile, &b->projTile, SHADER_UNIFORM_FLOAT, 1);
```

e nel blocco che rimette a posto dopo il disegno, accanto al ripristino di `alphaCut`:

```c
    /* Come per la soglia dell'alfa: un lotto non deve lasciare acceso qualcosa
     * per il successivo, che potrebbe avere UV vere. */
    if (b->locProjMode != -1 && b->projMode != 0) {
        int spento = 0;
        rlSetUniform(b->locProjMode, &spento, SHADER_UNIFORM_INT, 1);
    }
```

In fondo al modulo:

```c
void InstProjection(InstBatch *b, int mode, float tile)
{
    b->projMode = mode;
    b->projTile = (tile > 1e-4f) ? tile : 1.0f;
}

void InstModelProjection(InstModel *im, int mode, float tile)
{
    for (int i = 0; i < im->n; i++) InstProjection(im->b[i], mode, tile);
}
```

E in `src/instancing.h`, sotto le funzioni dell'alfa:

```c
/* Come si campiona la texture di questo lotto: 0 le UV della mesh, 1 la
 * proiezione sull'asse dominante, 2 la miscela a tre. 'tile' e' quanti metri
 * copre una ripetizione. Serve ai pezzi dei kit, che hanno UV inutilizzabili:
 * le loro coordinate stanno tutte in una cella della tavolozza. */
void InstProjection(InstBatch *b, int mode, float tile);
void InstModelProjection(InstModel *im, int mode, float tile);
```

- [ ] **Passo 6: lo stesso per il percorso non instanziato**

In `src/light.c`, accanto a `locAlphaCut`:

```c
static int locProjMode[PROG_COUNT], locProjTile[PROG_COUNT];
```

nel ciclo che cerca le posizioni degli uniform:

```c
        locProjMode[p]     = GetShaderLocation(gProg[p], "projMode");
        locProjTile[p]     = GetShaderLocation(gProg[p], "projTile");
```

e accanto a `LightSetAlphaCut()`:

```c
void LightSetProjection(int mode, float tile)
{
    for (int p = 0; p < PROG_COUNT; p++) {
        if (gProg[p].id == 0) continue;
        SetShaderValue(gProg[p], locProjMode[p], &mode, SHADER_UNIFORM_INT);
        SetShaderValue(gProg[p], locProjTile[p], &tile, SHADER_UNIFORM_FLOAT);
    }
}
```

In `src/light.h`, sotto `LightSetAlphaCut`:

```c
/* Come si campiona la texture: 0 le UV della mesh, 1 la proiezione sull'asse
 * dominante, 2 la miscela a tre; 'tile' e' quanti metri copre una ripetizione.
 * E' il gemello di InstProjection() per il percorso non instanziato, quello
 * che gira quando assets/shaders/ manca o un lotto non si e' creato. */
void LightSetProjection(int mode, float tile);
```

- [ ] **Passo 7: lanciare le prove e vederle passare**

Esegui: `make prove && make`
Atteso: `build/prove/proiezione` con tutte le righe `ok` (o saltata senza GPU), le altre prove invariate, `make` senza avvisi su Linux e Windows.

- [ ] **Passo 8: sabotare**

Una per volta, ricompilando e rilanciando `make prove`, poi rimettendo a posto:

1. in `CoordProiettata()`, usa `fragPosition` al posto di `fragLocal` (cioe' proietta in coordinate di mondo) → deve fallire "spazio oggetto: a 90 gradi il motivo gira con l'oggetto";
2. in `AsseDominante()`, torna sempre `0` → deve fallire il controllo sulla U che segue x e non z **oppure** quello sui salti, a seconda di come e' orientato il quadrato: riporta quale;
3. in `InstProjection()`, ignora `mode` e lascia `b->projMode = 0` → deve fallire "modo 1: il passo di 1 m ripete la texture";
4. in `CoordProiettata()`, togli `+ fragProjOffset` → deve fallire "lo sfalsamento cambia la fase fra istanze in posizioni diverse";
5. togli il ripristino di `projMode` dopo il disegno → le prove passano lo stesso, perche' qui il lotto e' uno solo. Annotalo come buco di copertura noto: non inventare una prova contorta per prenderlo.

- [ ] **Passo 9: commit**

```bash
git add assets/shaders src/light.h src/light.c src/instancing.h src/instancing.c tools/prove/proiezione.c Makefile
git commit -m "La texture dalla posizione, in spazio oggetto"
```

---

### Task 2: La miscela a tre per il tetto

**File:**
- Modifica: `assets/shaders/scene.fs`
- Modifica: `tools/prove/proiezione.c`

**Interfacce:**
- Consuma: `projMode`, `projTile`, `CoordProiettata()`, `InstProjection()` dal Task 1.
- Produce: il comportamento del modo 2. Nessuna firma nuova.

- [ ] **Passo 1: scrivere la prova che fallisce**

Le falde del tetto hanno la normale a circa 45 gradi: l'asse dominante oscilla lungo la pendenza e nasce una cucitura netta. Serve una mesh che attraversi quella soglia, quindi un quadrato **inclinato** con normali che spazzano da +X a +Y.

Aggiungi in `tools/prove/proiezione.c`, sopra `main()`:

```c
/* Striscia inclinata: la geometria e' piana ma le normali dei vertici vanno da
 * +X a +Y, quindi lungo la striscia la normale interpolata attraversa i 45
 * gradi - il punto in cui l'asse dominante cambia. E' la falda del tetto,
 * ridotta all'osso. */
static Mesh StrisciaObliqua(void)
{
    static float v[18]  = { -2,0,-2,  -2,0,2,   2,0,2,
                            -2,0,-2,   2,0,2,   2,0,-2 };
    /* x cresce da sinistra a destra: a sinistra la normale e' quasi +Y, a
     * destra quasi +X. */
    static float n[18]  = { 0.20f,0.98f,0,  0.20f,0.98f,0,  0.98f,0.20f,0,
                            0.20f,0.98f,0,  0.98f,0.20f,0,  0.98f,0.20f,0 };
    static float uv[12] = { 0,0, 0,1, 1,1, 0,0, 1,1, 1,0 };

    Mesh m = { 0 };
    m.vertexCount = 6;
    m.triangleCount = 2;
    m.vertices = v; m.normals = n; m.texcoords = uv;
    UploadMesh(&m, false);
    return m;
}

/* Quanti pixel del quadrato differiscono fra due rese. E' la misura giusta per
 * la miscela: dove la normale e' diagonale il modo 2 preleva TUTTE E TRE le
 * proiezioni e le pesa, quindi il risultato si scosta dal modo 1 su tutta la
 * superficie; dove la normale e' su un asse, il peso e' uno solo e i due modi
 * devono coincidere.
 *
 * Non si misura il gradino della cucitura, e la ragione va detta perche' e'
 * controintuitiva: sulla striscia l'asse cambia dove |nx| = |ny|, cioe' al
 * centro, e li' la proiezione X vale z mentre la Y vale x. Sulla riga centrale
 * z e x si equivalgono e il gradino e' NULLO: una prova che lo cercasse
 * passerebbe anche senza miscela. */
static int PixelDiversi(Image a, Image b)
{
    int n = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
            if (!SulQuadrato(ca) || !SulQuadrato(cb)) continue;
            if (abs((int)ca.r - (int)cb.r) > 12) n++;
        }
    return n;
}
```

e in `main()`, prima di `ProveEsito()`:

```c
    /* --- 5. il tetto: dove la normale e' diagonale serve la miscela ------- */
    Mesh obliqua = StrisciaObliqua();
    InstBatch *bo = InstCreate(obliqua, mat);
    Ok("lotto obliquo creato", bo != NULL);

    /* Passo largo: la rampa non deve ripetersi sulla striscia, o i suoi ritorni
     * a zero sporcherebbero il confronto fra le due rese. */
    InstProjection(bo, 1, 40.0f);
    Image imDom = Rendi(rt, bo, 0.0f);
    InstProjection(bo, 2, 40.0f);
    Image imMix = Rendi(rt, bo, 0.0f);
    int diversiObliqua = PixelDiversi(imDom, imMix);

    /* Sul quadrato piatto la normale e' esattamente +Y: il peso della miscela
     * e' tutto su una proiezione sola, quindi i due modi devono dare la stessa
     * immagine. E' il controllo che impedisce di far passare la miscela
     * scrivendo qualcosa che cambia il colore dappertutto. */
    InstProjection(b, 1, 40.0f);
    Image imPiattoDom = Rendi(rt, b, 0.0f);
    InstProjection(b, 2, 40.0f);
    Image imPiattoMix = Rendi(rt, b, 0.0f);
    int diversiPiatto = PixelDiversi(imPiattoDom, imPiattoMix);

    printf("  pixel diversi fra modo 1 e modo 2: obliqua %d, piatta %d\n",
           diversiObliqua, diversiPiatto);
    Ok("modo 2: sulla normale diagonale la miscela cambia il risultato",
       diversiObliqua > 500);
    Ok("modo 2: sulla normale su un asse la miscela non cambia niente",
       diversiPiatto < 50);

    UnloadImage(imDom); UnloadImage(imMix);
    UnloadImage(imPiattoDom); UnloadImage(imPiattoMix);
    InstFree(bo);
```

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: `modo 2: la miscela toglie la cucitura` FALLITO — il modo 2 oggi non esiste e `CoordProiettata()` si comporta come il modo 1, quindi i due salti sono uguali.

- [ ] **Passo 3: implementare la miscela**

In `assets/shaders/scene.fs`, sotto `CoordProiettata()`:

```c
/* Dove la normale e' diagonale l'asse dominante oscilla, e a meta' falda
 * nascerebbe un gradino netto. Lì si prelevano tutte e tre le proiezioni e si
 * pesano con il quadrato della normale: tre prelievi, su un tipo di pezzo
 * solo. Il quadrato invece del valore assoluto stringe la fascia in cui due
 * proiezioni si sovrappongono, e quindi la sfocatura. */
vec4 CampionaMiscelato(sampler2D tex)
{
    vec3 w = fragLocalNormal * fragLocalNormal;
    w /= max(w.x + w.y + w.z, 1e-4);
    float t = max(projTile, 1e-4);
    vec2 o = fragProjOffset;

    return texture(tex, (vec2(fragLocal.z, fragLocal.y) + o) / t) * w.x
         + texture(tex, (vec2(fragLocal.x, fragLocal.z) + o) / t) * w.y
         + texture(tex, (vec2(fragLocal.x, fragLocal.y) + o) / t) * w.z;
}
```

e in `main()` sostituisci le due righe dell'albedo con:

```glsl
    vec4 base = (projMode == 2) ? CampionaMiscelato(texture0)
                                : texture(texture0, (projMode == 0) ? fragTexCoord
                                                                    : CoordProiettata());
    vec4 albedo = base * colDiffuse * fragColor;
```

- [ ] **Passo 4: lanciare le prove e vederle passare**

Esegui: `make prove && make`
Atteso: tutte `ok`, zero avvisi.

- [ ] **Passo 5: sabotare**

1. fai campionare al modo 2 la stessa `CoordProiettata()` del modo 1, cioe' salta la miscela → deve fallire "modo 2: sulla normale diagonale la miscela cambia il risultato";
2. in `CampionaMiscelato()`, sostituisci il quadrato della normale con il valore assoluto → le prove passano lo stesso: e' un'altra pesatura legittima, solo piu' morbida. Annotalo, non e' un fallimento.

- [ ] **Passo 6: commit**

```bash
git add assets/shaders/scene.fs tools/prove/proiezione.c
git commit -m "La miscela a tre dove la normale e' diagonale"
```

---

### Task 3: La normal map sotto proiezione

**File:**
- Modifica: `assets/shaders/scene.fs`
- Modifica: `tools/prove/proiezione.c`

**Interfacce:**
- Consuma: `projMode`, `fragLocal`, `fragLocalNormal`, `fragYawSC`, `fragInvScale` dal Task 1.
- Produce: `SurfaceNormal()` che, sotto proiezione, costruisce la terna dagli assi della proiezione. Nessuna firma nuova.

- [ ] **Passo 1: scrivere la prova che fallisce**

Senza normal map il legno scuro e' una macchia scura: la resa sta li'. La prova deve dimostrare che il rilievo **segue l'oggetto**, cioe' che ruotando l'istanza il lato illuminato ruota con lei.

Aggiungi in `tools/prove/proiezione.c`, sopra `main()`:

```c
/* Normal map costante che inclina la normale verso +U di circa 30 gradi.
 * In spazio tangente il vettore (0.5, 0, 0.87) codificato in RGB e'
 * (191, 128, 222): il rosso e' la componente lungo la tangente. */
static Texture2D NormaleInclinata(void)
{
    Image im = GenImageColor(4, 4, (Color){ 191, 128, 222, 255 });
    Texture2D t = LoadTextureFromImage(im);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(im);
    return t;
}

/* Luminosita' media dei pixel accesi. */
static float Luminosita(Image im)
{
    double s = 0.0; int n = 0;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++) {
            Color c = GetImageColor(im, x, y);
            if (c.r + c.g + c.b < 12) continue;
            s += c.r + c.g + c.b; n++;
        }
    return (n > 0) ? (float)(s / (3.0 * n)) : 0.0f;
}
```

e in `main()`, prima di `ProveEsito()`:

```c
    /* --- 6. la normal map segue l'oggetto -------------------------------- */
    /* Sole radente lungo +X: con la normale inclinata verso la tangente, il
     * quadrato e' piu' chiaro quando l'inclinazione punta verso il sole. */
    LightSetSun((Vector3){ 0.94f, 0.34f, 0.0f }, 1.0f);
    LightFrame(nulla);

    Material rilievo = LoadMaterialDefault();
    LightApplyToMaterial(&rilievo);
    rilievo.maps[MATERIAL_MAP_DIFFUSE].texture = Rampa();
    rilievo.maps[MATERIAL_MAP_NORMAL].texture  = NormaleInclinata();

    InstBatch *br = InstCreate(q, rilievo);
    Ok("lotto con rilievo creato", br != NULL);
    InstProjection(br, 1, 2.0f);

    Image imA = Rendi(rt, br, 0.0f);
    Image imB = Rendi(rt, br, 180.0f);
    float lumA = Luminosita(imA), lumB = Luminosita(imB);
    printf("  luminosita': a 0 gradi %.1f, a 180 gradi %.1f\n",
           (double)lumA, (double)lumB);
    Ok("il rilievo gira con l'oggetto: 0 e 180 gradi non si illuminano uguale",
       fabsf(lumA - lumB) > 4.0f);

    UnloadImage(imA); UnloadImage(imB);
    InstFree(br);
```

- [ ] **Passo 2: lanciarla e vederla fallire**

Esegui: `make prove`
Atteso: FALLITO. La mesh della prova non porta tangenti, quindi `SurfaceNormal()` scarta la normal map e resta alla normale del vertice: le due luminosita' sono identiche.

- [ ] **Passo 3: costruire la terna dagli assi della proiezione**

In `assets/shaders/scene.fs`, sostituisci `SurfaceNormal()` con una versione che si sdoppia. La parte esistente resta parola per parola nel ramo delle UV; il ramo nuovo e' questo:

```glsl
/* Sotto proiezione la tangente del vertice non serve: la terna si costruisce
 * dagli ASSI DELLA PROIEZIONE, che sono gli assi dell'oggetto. La normale
 * perturbata nasce quindi in spazio oggetto e va riportata in mondo con la
 * stessa regola che il vertex shader usa per le normali: dividere per la scala
 * e ruotare attorno a Y. Dividere, non moltiplicare: su una scala non uniforme
 * moltiplicare darebbe normali storte. */
vec3 RuotaYFrag(vec3 v)
{
    float s = fragYawSC.x, c = fragYawSC.y;
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

vec3 NormaleProiettata()
{
    int asse = AsseDominante(fragLocalNormal);
    vec3 n = normalize(fragLocalNormal);

    /* La tangente e' l'asse lungo cui corre la U della proiezione. */
    vec3 t = (asse == 1) ? vec3(0.0, 0.0, 1.0)
           : (asse == 2) ? vec3(1.0, 0.0, 0.0)
                         : vec3(1.0, 0.0, 0.0);
    t = normalize(t - n * dot(n, t));
    if (dot(t, t) < 1e-8) return normalize(RuotaYFrag(n * fragInvScale));

    vec3 b  = cross(n, t);
    vec2 uv = (projMode == 2) ? ProiettaUV(fragLocal, asse) / max(projTile, 1e-4)
                              : CoordProiettata();
    vec3 ts = texture(texture2, uv).rgb * 2.0 - 1.0;

    vec3 nObj = normalize(mat3(t, b, n) * ts);
    return normalize(RuotaYFrag(nObj * fragInvScale));
}
```

e in `main()` sostituisci la riga della normale:

```glsl
    vec3  n    = (projMode == 0) ? SurfaceNormal() : NormaleProiettata();
```

- [ ] **Passo 4: lanciare le prove e vederle passare**

Esegui: `make prove && make`
Atteso: tutte `ok`, zero avvisi su Linux e Windows.

- [ ] **Passo 5: sabotare**

1. in `NormaleProiettata()`, togli `RuotaYFrag(...)` e torna `nObj` → deve fallire "il rilievo gira con l'oggetto";
2. togli `* fragInvScale` → le prove passano lo stesso, perche' la prova disegna a scala unitaria. Annotalo come buco di copertura noto: la scala non uniforme si vede solo sul tetto, in gioco;
3. in `NormaleProiettata()`, usa `fragTexCoord` al posto di `uv` → deve fallire, perche' la mesh della prova ha UV che coprono 0..1 e la normale letta cambia di poco: se non fallisce, riportalo invece di allentare la soglia.

- [ ] **Passo 6: commit**

```bash
git add assets/shaders/scene.fs tools/prove/proiezione.c
git commit -m "Il rilievo dagli assi della proiezione, non dalle tangenti"
```

---

### Task 4: Scaricare i materiali

**File:**
- Crea: `tools/polyhaven_tex.py`
- Modifica: `tools/fetch_assets.sh`

**Interfacce:**
- Consuma: niente dai task precedenti.
- Produce: `./tools/fetch_assets.sh texture <asset> <nome>`, che lascia in `assets/textures/` i file `<nome>_diff.jpg` e `<nome>_nor.jpg`.

- [ ] **Passo 1: scrivere `tools/polyhaven_tex.py`**

`polyhaven_get.py` cerca il glTF di un **modello**; per un materiale la struttura del JSON e' diversa: le mappe stanno in cima (`Diffuse`, `nor_gl`, `Rough`, `arm`), ognuna per risoluzione e formato.

```python
#!/usr/bin/env python3
"""Scarica un materiale di Poly Haven: albedo e normal map.

    polyhaven_tex.py <files.json> <cartella> <nome>

Lascia <nome>_diff.jpg e <nome>_nor.jpg. Il primo argomento e' la risposta di
https://api.polyhaven.com/files/<asset> gia' salvata, come per i modelli: cosi'
lo script non decide da solo cosa scaricare.

Si prende sempre 1k in jpg. Gli edifici si guardano da qualche metro e la
proiezione ripete la texture ogni paio di metri: 4k sarebbe peso senza
differenza. La normale e' la variante OpenGL - 'nor_gl' - perche' e' la
convenzione che scene.fs si aspetta, con la Y verso l'alto; 'nor_dx' darebbe il
rilievo ribaltato.

Come gli altri strumenti del repo: nessuna dipendenza oltre alla standard.
"""
import json
import os
import sys
import urllib.request


def scarica(url, dove):
    os.makedirs(os.path.dirname(dove) or ".", exist_ok=True)
    urllib.request.urlretrieve(url, dove)
    print(f"  {os.path.getsize(dove) / 1e6:6.2f} MB  {os.path.basename(dove)}")


def mappa(d, chiave):
    v = d.get(chiave, {}).get("1k", {})
    return v.get("jpg", {}).get("url")


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2

    dati, cartella, nome = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(dati, encoding="utf-8") as f:
        d = json.load(f)

    diff = mappa(d, "Diffuse")
    nor = mappa(d, "nor_gl")
    if diff is None:
        print("questo asset non ha una mappa Diffuse a 1k in jpg: mi fermo")
        return 1

    scarica(diff, os.path.join(cartella, f"{nome}_diff.jpg"))
    if nor is None:
        print("  senza normal map: il materiale restera' piatto")
    else:
        scarica(nor, os.path.join(cartella, f"{nome}_nor.jpg"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Passo 2: il ramo nello script**

In `tools/fetch_assets.sh`, subito dopo il blocco `polyhaven`/`rocce`, aggiungi:

```bash
# ---- materiali (Poly Haven) ------------------------------------------------
#  I pezzi modulari dei kit non hanno UV utilizzabili - le loro coordinate
#  stanno tutte in una cella della tavolozza - quindi la loro texture si
#  PROIETTA dalla posizione. Servono percio' materiali, non modelli.
#
#      ./tools/fetch_assets.sh texture <asset> <nome>
#
#  Lascia assets/textures/<nome>_diff.jpg e <nome>_nor.jpg.
if [ "${1:-}" = "texture" ]; then
    for cmd in curl python3; do
        command -v "$cmd" >/dev/null 2>&1 || { echo "serve $cmd"; exit 1; }
    done

    ASSET="${2:-}"; DEST="${3:-}"
    [ -n "$ASSET" ] && [ -n "$DEST" ] || {
        echo "uso: $0 texture <asset> <nome>"; exit 1; }

    echo "cerco il materiale $ASSET su polyhaven.com..."
    curl -sSL "https://api.polyhaven.com/files/$ASSET" -o "$ASSETS/.ph.json"
    python3 "$ROOT/tools/polyhaven_tex.py" "$ASSETS/.ph.json" "$ASSETS/textures" "$DEST"
    rm -f "$ASSETS/.ph.json"

    if ! grep -q "assets/textures/${DEST}_diff.jpg" "$ASSETS/CREDITS.md" 2>/dev/null; then
        printf '| assets/textures/%s_diff.jpg | Poly Haven | https://polyhaven.com/a/%s | CC0 | %s |\n' \
            "$DEST" "$ASSET" "$(date +%Y-%m-%d)" >> "$ASSETS/CREDITS.md"
        echo "  aggiunta la riga in assets/CREDITS.md"
    fi
    exit 0
fi
```

- [ ] **Passo 3: provarlo davvero**

```bash
./tools/fetch_assets.sh texture dark_wooden_planks legno_scuro
ls -la assets/textures/legno_scuro_*.jpg
```

Atteso: due file, `legno_scuro_diff.jpg` e `legno_scuro_nor.jpg`, e una riga nuova in `assets/CREDITS.md`. Se l'asset non esistesse, lo script deve dirlo e uscire non-zero invece di lasciare file a meta'.

Scarica anche gli altri tre, che servono al Task 5:

```bash
./tools/fetch_assets.sh texture roof_planks        tetto_legno
./tools/fetch_assets.sh texture plank_flooring     assito
./tools/fetch_assets.sh texture castle_wall_slates pietra
```

- [ ] **Passo 4: commit**

`assets/textures/` e' in `.gitignore`: entrano nel commit solo lo script e i crediti.

```bash
git add tools/polyhaven_tex.py tools/fetch_assets.sh assets/CREDITS.md
git commit -m "Scaricare un materiale, non solo un modello"
```

---

### Task 5: Accendere gli edifici

**File:**
- Modifica: `src/world.c` (la tabella `BUILD_FILES` e `LoadBuildParts`, `PlacePart`)
- Modifica: `docs/01-architettura.md`, `docs/03-asset-pubblici.md`

**Interfacce:**
- Consuma: `InstModelProjection()` dal Task 1, `LightSetProjection()` dal Task 1, i quattro materiali scaricati nel Task 4.
- Produce: nessuna API nuova.

- [ ] **Passo 1: la tabella dei materiali**

In `src/world.c`, sotto `BUILD_FILES`:

```c
/* Che materiale va su ogni pezzo, con che passo e con che modo di proiezione.
 * I pezzi dei kit non hanno UV utilizzabili - il muro ha 64 vertici e tutte le
 * sue coordinate stanno in una cella della tavolozza - quindi la texture si
 * ricava dalla posizione.
 *
 * Il passo e' per INSIEME e non per pezzo: le assi di un muro e quelle di una
 * finestra devono avere lo stesso, o la finestra si stacca dalla parete.
 *
 * Il tetto e' l'unico a modo 2: le sue falde hanno la normale a 45 gradi, e
 * con l'asse dominante nascerebbe una cucitura a meta' falda. */
typedef struct { const char *nome; float tile; int mode; } BuildMat;

static const BuildMat gBuildMat[BUILD_PART_COUNT] = {
    [BUILD_WALL]        = { "legno_scuro", 2.0f, 1 },
    [BUILD_DOOR]        = { "legno_scuro", 2.0f, 1 },
    [BUILD_WINDOW]      = { "legno_scuro", 2.0f, 1 },
    [BUILD_ROOF]        = { "tetto_legno", 1.5f, 2 },
    [BUILD_FLOOR]       = { "assito",      2.0f, 1 },
    [BUILD_STAIRS]      = { "assito",      2.0f, 1 },
    [BUILD_TOWER_BASE]  = { "pietra",      2.5f, 1 },
    [BUILD_TOWER_MID]   = { "pietra",      2.5f, 1 },
    [BUILD_TOWER_TOP]   = { "pietra",      2.5f, 1 },
    [BUILD_TOWER_ROOF]  = { "pietra",      2.5f, 1 },
};
```

- [ ] **Passo 2: montare le texture sui pezzi**

In `LoadBuildParts()`, dopo `LightApplyToModel(&w->buildPart[i])` e prima di `InstModelCreate(&w->partBatch[i], ...)`:

```c
        /* Il materiale proiettato sostituisce la tavolozza del kit. Se i file
         * non ci sono si resta alla tavolozza e al modo 0: il gioco funziona
         * senza assets/, e questo non e' un caso d'errore. */
        char diff[128], nor[128];
        snprintf(diff, sizeof diff, "assets/textures/%s_diff.jpg", gBuildMat[i].nome);
        snprintf(nor,  sizeof nor,  "assets/textures/%s_nor.jpg",  gBuildMat[i].nome);

        if (FileExists(diff)) {
            Texture2D td = LoadTexture(diff);
            SetTextureFilter(td, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(td, TEXTURE_WRAP_REPEAT);
            GenTextureMipmaps(&td);
            for (int k = 0; k < w->buildPart[i].materialCount; k++)
                w->buildPart[i].materials[k].maps[MATERIAL_MAP_DIFFUSE].texture = td;

            if (FileExists(nor)) {
                Texture2D tn = LoadTexture(nor);
                SetTextureFilter(tn, TEXTURE_FILTER_TRILINEAR);
                SetTextureWrap(tn, TEXTURE_WRAP_REPEAT);
                GenTextureMipmaps(&tn);
                for (int k = 0; k < w->buildPart[i].materialCount; k++)
                    w->buildPart[i].materials[k].maps[MATERIAL_MAP_NORMAL].texture = tn;
            }
            w->buildProj[i] = true;
        }
```

Aggiungi il campo in `src/world.h`, accanto a `partBatch`:

```c
    /* Quali pezzi hanno ricevuto un materiale proiettato: gli altri restano
     * alla tavolozza del kit e al modo 0. */
    bool   buildProj[BUILD_PART_COUNT];
```

E subito dopo `InstModelCreate(&w->partBatch[i], w->buildPart[i]);`:

```c
        if (w->buildProj[i])
            InstModelProjection(&w->partBatch[i], gBuildMat[i].mode, gBuildMat[i].tile);
```

- [ ] **Passo 3: il ripiego non instanziato**

In `PlacePart()` (`src/world.c`), il ramo `else` disegna con `DrawModelEx` e non passa da nessun lotto: l'uniform va messo a mano, come si fa gia' per la soglia dell'alfa nei prop.

```c
    if (InstModelReady(&w->partBatch[part]))
        InstModelAdd(&w->partBatch[part], p, rotDeg + localRot, scale);
    else {
        /* Ripiego: si arriva qui quando manca assets/shaders/ o un lotto non
         * si e' creato. L'uniform e' per lotto, e qui di lotto non ce n'e'. */
        if (w->buildProj[part])
            LightSetProjection(gBuildMat[part].mode, gBuildMat[part].tile);
        DrawModelEx(w->buildPart[part], p, (Vector3){ 0.0f, 1.0f, 0.0f },
                    rotDeg + localRot, scale, tint);
        if (w->buildProj[part]) LightSetProjection(0, 1.0f);
    }
```

- [ ] **Passo 4: scaricare le texture del mondo e guardare**

```bash
make && make prove && make valida
timeout 15 ./frostmark > /tmp/run.log 2>&1
grep -E "modello esterno|WARNING" /tmp/run.log | head
```

Poi **guarda il gioco**, che nessuna prova puo' fare al posto tuo:

1. i muri delle case hanno assi di legno alla scala giusta, non una macchia marrone;
2. **il tetto non ha una cucitura a meta' falda** — e' il Task 2 messo alla prova sul modello vero;
3. una casa **ruotata** ha le assi verticali come una non ruotata: e' la ragione dello spazio oggetto;
4. due case vicine non hanno la venatura identica nello stesso punto;
5. la torre e' di pietra e non di legno.

Se qualcosa non torna, riporta con il numero — quale passo, quale pezzo — invece di ritarare la tabella a tentativi.

- [ ] **Passo 5: i documenti**

- `docs/01-architettura.md`: una sezione *Materiali proiettati* accanto a *Varianti*, che dica la regola (asse dominante, spazio oggetto, miscela solo sul tetto), perche' la posizione locale e' moltiplicata per la scala, e che l'interruttore viaggia per lotto come `alphaCut`.
- `docs/03-asset-pubblici.md`: che i kit modulari non hanno UV utilizzabili e come si riconosce il caso (guardare l'intervallo delle UV: se stanno in una finestrella dell'atlante, e' una tavolozza); e i quattro materiali scelti con il loro passo.

- [ ] **Passo 6: verifica finale e commit**

```bash
make && make prove && make valida
git add src/world.c src/world.h docs
git commit -m "Assi di legno sulle case, pietra sulla torre"
```

---

## Note per chi esegue

- **Il percorso non instanziato non e' codice morto:** gira quando `assets/shaders/` manca. Verificalo a mano rinominando `assets/shaders/scene_inst.vs` e riavviando: le case devono restare con le loro assi, non tornare alla tavolozza.
- **Non toccare `BUILD_CELL`.** Se un materiale sembra fuori scala, si cambia il passo nella tabella, non la dimensione delle celle: la geometria e la collisione dipendono da quella.
- **Il mondo cotto non va rigenerato.** Niente di quello che fa questo piano tocca i dati su disco.
- Se un uniform nuovo risulta a -1, non e' un bug del C: e' il compilatore GLSL che lo ha eliminato perche' nessun ramo dello shader lo usa. Si aggiunge l'uso, non un aggiramento.
