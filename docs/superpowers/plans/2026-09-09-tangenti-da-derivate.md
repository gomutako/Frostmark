# Le tangenti dalle derivate di schermo — piano di implementazione

> **Per chi esegue:** SOTTO-SKILL RICHIESTA: usare
> `superpowers:subagent-driven-development` (consigliata) oppure
> `superpowers:executing-plans` per eseguire questo piano un compito alla
> volta. I passi usano caselle (`- [ ]`) per il tracciamento.

**Obiettivo:** `SurfaceNormal()` in `assets/shaders/scene.fs` costruisce la
terna tangente/bitangente/normale dalle **derivate di schermo** invece che
dall'attributo `fragTangent`, così che una mesh animata abbia il rilievo
corretto senza che nessuno debba aggiornarle le tangenti.

**Architettura:** un solo blocco di codice cambia, nel ramo `projMode == 0` del
fragment shader. La terna si ricava da `dFdx`/`dFdy` della posizione nel mondo
e delle coordinate texture — il *cotangent frame* — che su una mesh animata è
corretto per costruzione, perché `fragPosition` esce dal vertex shader dopo
`matModel` e quindi le derivate leggono posizioni già deformate dallo
scheletro. I due rami proiettati non si toccano. Prima del codice si estende la
prova, che oggi non morderebbe.

**Tecnologie:** GLSL 3.30, C99, raylib 5.5, `make`.

**Spec:** `docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md`

## Vincoli globali

- **Zero avvisi.** Il progetto compila con `-Wall -Wextra` e non tollera
  avvisi, né su Linux né su Windows. `make` deve restare pulito.
- **Le prove si lanciano dalla radice del repo**: caricano gli asset per
  percorso relativo.
- **Chi esce 77 è saltato, non fallito**: nessun contesto OpenGL.
- **I valori attesi si calcolano a mano prima di guardarli.** È la regola
  scritta nell'intestazione di `tools/prove/normalmap.c`, e vale anche per la
  soglia della misura di costo.
- **Un effetto per misura.** Nessun altro cambiamento entra nei commit fra il
  compito 2 e il compito 5, o il numero del costo non è più attribuibile.
- **Niente codice speculativo.** La mitigazione del rumore e l'interruttore per
  lotto si scrivono solo se una misura li chiama.

## Struttura dei file

| file | responsabilità | compiti |
|---|---|---|
| `tools/prove/normalmap.c` | la prova: due casi nuovi che discriminano la bitangente e l'orientamento della tangente | 1, 3 |
| `assets/shaders/scene.fs` | `SurfaceNormal()`: la terna dalle derivate | 2 |
| la spec | i numeri misurati, la soglia dichiarata prima | 4, 5 |
| `docs/01-architettura.md` | sezioni *Normal map* e *Le prove* | 6 |
| `docs/06-stato-e-prossimi-passi.md` | la domanda F | 6 |
| `src/light.c`, `src/light.h`, `src/game.c` | **solo se** la misura sfora: l'interruttore acceso intorno ai personaggi | 5b |

---

## Compito 0: il ramo

- [ ] **Passo 1: creare il ramo**

Il repo è su `main` e i compiti 1 e 2 committano separatamente una prova rossa
e la sua riparazione. Su `main` questo lascerebbe `make prove` rotto fra i due
commit.

```bash
git checkout -b tangenti-derivate
```

---

## Compito 1: la prova che morde (ROSSA)

La prova di oggi è cieca al verso della bitangente per due ragioni
indipendenti: le tre normal map hanno il verde a 128, quindi `ts.y ≈ 0`; e il
sole è `(0.6, 0.8, 0.0)` mentre la bitangente del quadrato sta su ±Z. Il
sabotaggio prescritto dalla spec non fallirebbe. Prima del codice va estesa.

**File:**
- Modifica: `tools/prove/normalmap.c`

**Interfacce:**
- Consuma: `Quadrato()`, `PixelNormale()`, `Centro()`, `Near()`, `LightSetSun()`,
  `LightFrame()` — tutte già nel file.
- Produce: `QuadratoUVRuotate()` — `static Mesh QuadratoUVRuotate(void)`, un
  quadrato nel piano XZ con normale +Y, u lungo +Z, v lungo +X, e la tangente
  dichiarata a mano `(1,0,0)` con verso `+1`, cioè **volutamente in disaccordo
  con le proprie UV**.

- [ ] **Passo 1: aggiungere la nota sul perché il fixture è incoerente**

Va prima del codice, o qualcuno "riparerà" `Quadrato()` e ucciderà il caso in
silenzio. Aggiungere in coda all'intestazione del file, prima di `#include`:

```c
/* --- Perche' due fixture dichiarano tangenti SBAGLIATE ----------------------
 * Quadrato() dichiara w = +1, che da' b = cross(n,t) = (0,0,-1). Ma le sue UV
 * fanno crescere la v lungo +Z, quindi la bitangente vera e' (0,0,+1): la w
 * dichiarata e' in disaccordo con le UV dello stesso quadrato.
 *
 * NON e' un errore da correggere: e' cio' che rende discriminanti i casi 6a e
 * 6b. Una terna costruita dalle derivate segue le UV e ignora la w; una
 * costruita dall'attributo segue la w. Con le due d'accordo i due percorsi
 * darebbero lo stesso numero e la prova non distinguerebbe niente.
 *
 * E' anche il caso vero: una mesh animata porta una tangente che non
 * corrisponde piu' alla sua superficie, perche' UpdateModelAnimation() non la
 * aggiorna. Un fixture statico con la tangente in disaccordo con le UV e' il
 * sostituto piu' vicino che si possa provare senza un personaggio in gioco.
 * ------------------------------------------------------------------------ */
```

- [ ] **Passo 2: aggiungere il fixture con le UV ruotate**

Subito dopo `QuadratoIndicizzato()`:

```c
/* Lo stesso quadrato, ma con le UV RUOTATE di 90 gradi: la u cresce lungo +Z e
 * la v lungo +X, quindi la tangente vera e' (0,0,1) e la bitangente (1,0,0).
 * La tangente dichiarata resta (1,0,0) con verso +1 - quella del quadrato non
 * ruotato - cioe' e' esattamente il valore che avrebbe un'implementazione che
 * inchiodasse la tangente a +X. Serve a distinguere una terna che segue
 * davvero le UV da una che sembra funzionare perche' in ogni altro quadrato
 * del file la u cresce gia' lungo +X. */
static Mesh QuadratoUVRuotate(void)
{
    static float v[18]  = { -2,0,-2,  -2,0,2,   2,0,2,
                            -2,0,-2,   2,0,2,   2,0,-2 };
    static float n[18]  = { 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0 };
    static float uv[12] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
    static float tg[24] = { 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1 };

    Mesh m = { 0 };
    m.vertexCount = 6;
    m.triangleCount = 2;
    m.vertices = v; m.normals = n; m.texcoords = uv; m.tangents = tg;
    UploadMesh(&m, false);
    return m;
}
```

- [ ] **Passo 3: aggiungere la sezione 6 in `main()`**

In fondo a `main()`, subito prima di `CloseWindow();`:

```c
    /* --- 6. la terna segue le UV, non l'attributo ------------------------- */
    /* Sole con componente Z: senza, la bitangente sta su ±Z e il prodotto
     * scalare con il sole e' nullo in ogni caso - la prova non morderebbe
     * comunque, qualunque normal map le si dia. */
    LightSetSun((Vector3){ 0.0f, 0.8f, 0.6f }, 1.0f);
    LightFrame(nulla);
    /* LightFrame() ricarica shadowOn dal suo stato: vanno rispente a mano, o
     * il fattore d'ombra rientra nel conto e i numeri non tornano. */
    SetShaderValue(gShader, GetShaderLocation(gShader, "shadowOn"),  &zero, SHADER_UNIFORM_INT);
    SetShaderValue(gShader, GetShaderLocation(gShader, "depthOnly"), &zero, SHADER_UNIFORM_INT);

    /* 6a. Perturbazione lungo la BITANGENTE: ts = (0, 0.5, 0.866).
     *   terna dalle UV  b = (0,0,+1) -> n = (0, 0.866,  0.5), diff 0.993 -> 129
     *   terna dalla w   b = (0,0,-1) -> n = (0, 0.866, -0.5), diff 0.393 ->  78 */
    UnloadTexture(mat.maps[MATERIAL_MAP_NORMAL].texture);
    mat.maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(128, 191, 238);
    Near("bitangente: dalle UV, non dalla w", Centro(rt, q, mat), 129, 3);

    /* 6b. UV ruotate, perturbazione lungo la TANGENTE: ts = (0.5, 0, 0.866).
     *   terna dalle UV  t = (0,0,1) -> n = (0,   0.866, 0.5), diff 0.993 -> 129
     *   tangente a +X   t = (1,0,0) -> n = (0.5, 0.866, 0  ), diff 0.693 -> 104 */
    Mesh qr = QuadratoUVRuotate();
    UnloadTexture(mat.maps[MATERIAL_MAP_NORMAL].texture);
    mat.maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(191, 128, 238);
    Near("tangente: dalle UV, non inchiodata a +X", Centro(rt, qr, mat), 129, 3);
```

- [ ] **Passo 4: costruire ed eseguire — deve FALLIRE**

```bash
make prove 2>&1 | tail -40
```

Atteso: `normalmap` stampa

```
bitangente: dalle UV, non dalla w        atteso 129, letto  78  FALLITO
tangente: dalle UV, non inchiodata a +X  atteso 129, letto 104  FALLITO
```

e `make prove` esce non-zero con `==> PROVE FALLITE`.

**Se i due numeri letti non sono 78 e 104**, fermarsi e capire perché prima di
proseguire: significa che il modello mentale della terna attuale è sbagliato, e
il compito 2 riparerebbe la cosa sbagliata.

**Se escono 77 (saltata)**: non c'è contesto OpenGL su questa macchina. Questo
piano non è eseguibile qui — serve una macchina con GPU accessibile.

- [ ] **Passo 5: verificare che i casi vecchi siano intatti**

Nella stessa uscita, i tre valori storici devono ancora leggere 113, 129 e 78,
e i controlli della sezione 5 devono passare. Se uno di questi è cambiato, il
codice della prova ha toccato qualcosa che non doveva — probabilmente il sole,
che va cambiato **solo** nella sezione 6.

- [ ] **Passo 6: commit**

```bash
git add tools/prove/normalmap.c
git commit -m "La prova distingue la terna dalle UV da quella dall'attributo

Due casi nuovi, rossi: la prova di oggi e' cieca al verso della
bitangente perche' le tre normal map hanno il verde a 128 e il sole non
ha componente Z. Con le due bitangenti opposte di Quadrato() e
QuadratoIndicizzato() che passano entrambe con 129, il sabotaggio
prescritto non fallirebbe.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 2: la terna dalle derivate (VERDE)

**File:**
- Modifica: `assets/shaders/scene.fs`, la funzione `SurfaceNormal()` e il
  commento che la precede

**Interfacce:**
- Consuma: `fragPosition` e `fragTexCoord`, già dichiarati come `in` nel file e
  scritti in spazio mondo da entrambi i vertex shader (`scene.vs:41`,
  `scene_inst.vs:86-89`).
- Produce: `SurfaceNormal()` con la stessa firma `vec3 SurfaceNormal()`.
  `NormaleProiettata()` e `main()` non cambiano di una riga.

- [ ] **Passo 1: sostituire il commento che precede `SurfaceNormal()`**

Il commento attuale — quello che comincia con «La normale del frammento» e
finisce con «il conto e' sempre lo stesso» — descrive il percorso delle
tangenti. Sostituirlo per intero con:

```glsl
/* La normale del frammento. Quella del vertice descrive la forma grossa; la
 * normal map aggiunge il rilievo che la mesh non ha - la corteccia, la fuga fra
 * due pietre - ed e' meta' di cio' che fa sembrare realistico un asset.
 *
 * La mappa e' in spazio tangente, cioe' relativa alla superficie: per usarla
 * serve la terna (tangente, bitangente, normale). Quella terna NON viene
 * dall'attributo del vertice ma dalle DERIVATE DI SCHERMO - il cotangent
 * frame: dFdx e dFdy della posizione nel mondo e delle UV dicono come si
 * muovono le une rispetto alle altre, e risolvere il sistema 2x2 da' gli assi
 * della texture sulla superficie.
 *
 * Il motivo e' l'animazione. UpdateModelAnimation() di raylib aggiorna
 * posizioni e normali ma NON le tangenti: un personaggio con una normal map
 * vera avrebbe il rilievo fermo alla posa di riposo. Le derivate lavorano su
 * fragPosition, che esce dal vertex shader dopo matModel, cioe' su posizioni
 * gia' deformate dallo scheletro - quindi non esiste una tangente da
 * aggiornare, e la terna e' corretta per costruzione.
 *
 * Vale ovunque, non solo sugli animati: due percorsi che fanno la stessa cosa
 * in modi diversi divergono in silenzio, ed e' gia' successo qui con lo
 * sfalsamento della proiezione.
 *
 * Quello che le derivate NON risolvono sono le UV degeneri: li' le derivate
 * delle UV sono nulle, il determinante e' zero e la terna esce indefinita. Il
 * ripiego e' lo stesso di prima - la normale del vertice - su una condizione
 * diversa. Il problema si sposta dal vertice al frammento, non sparisce.
 *
 * I materiali senza normal map ne ricevono una piatta da light.c, quindi qui
 * non serve sapere se ce n'e' una vera: il conto e' sempre lo stesso. E su
 * quelli la terna non conta affatto, perche' mat3(t,b,n) * (0,0,1) == n
 * qualunque siano t e b. */
```

- [ ] **Passo 2: sostituire il corpo di `SurfaceNormal()`**

```glsl
vec3 SurfaceNormal()
{
    vec3 n = normalize(fragNormal);

    vec3 dp1 = dFdx(fragPosition);
    vec3 dp2 = dFdy(fragPosition);
    vec2 du1 = dFdx(fragTexCoord);
    vec2 du2 = dFdy(fragTexCoord);

    /* Il determinante del sistema 2x2. Zero significa UV degeneri - un
     * triangolo che sull'atlante e' un punto o un segmento - e li' non esiste
     * nessuna terna: si resta alla normale del vertice. */
    float det = du1.x * du2.y - du2.x * du1.y;
    if (abs(det) < 1e-12) return n;

    vec3 t = ( du2.y * dp1 - du1.y * dp2) / det;
    vec3 b = (-du2.x * dp1 + du1.x * dp2) / det;

    /* Il raddrizzamento rispetto alla normale: le derivate danno gli assi
     * della texture sul piano del triangolo, che su una superficie liscia non
     * e' il piano della normale interpolata. */
    t = t - n * dot(n, t);
    b = b - n * dot(n, b);
    if (dot(t, t) < 1e-12 || dot(b, b) < 1e-12) return n;
    t = normalize(t);
    b = normalize(b);

    vec3 ts = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(mat3(t, b, n) * ts);
}
```

Nota per chi esegue: la bitangente **non** si ricava da `cross(n, t)`. È lo
stesso errore che `NormaleProiettata()` documenta poche righe più sotto — il
prodotto vettore dà una terna destrorsa, non la direzione in cui cresce la V, e
i due coincidono solo su metà degli orientamenti. Qui la V la dicono le
derivate, e va usata quella.

- [ ] **Passo 3: costruire ed eseguire — deve PASSARE**

```bash
make prove 2>&1 | tail -40
```

Atteso: tutti e cinque i valori di `normalmap` — 113, 129, 78 per i casi
storici, 129 e 129 per i due nuovi — dentro tolleranza 3, e `==> prove passate`.

- [ ] **Passo 4: verificare che il resto non si sia mosso**

```bash
make 2>&1 | grep -i "warning\|error" ; echo "avvisi: $?"
```

Atteso: nessuna riga di avviso (l'`echo` stampa `avvisi: 1`, cioè `grep` non ha
trovato niente). Le prove `proiezione`, `alfa`, `luce` e `instancing` devono
restare verdi: toccano lo stesso shader ma non `SurfaceNormal()`.

- [ ] **Passo 5: commit**

```bash
git add assets/shaders/scene.fs
git commit -m "La terna nasce dalle derivate, non dall'attributo

SurfaceNormal() costruisce il cotangent frame da dFdx/dFdy invece di
leggere fragTangent. Su una mesh animata e' corretto per costruzione: le
derivate lavorano su fragPosition, cioe' su posizioni gia' deformate
dallo scheletro, quindi non c'e' nessuna tangente da aggiornare.

Ovunque e non solo sugli animati: due percorsi che fanno la stessa cosa
divergono in silenzio.

La bitangente viene dalle derivate e non da cross(n,t), per la stessa
ragione gia' documentata in NormaleProiettata().

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 3: i sabotaggi

Una prova che non prende niente è peggio di nessuna prova, e in questo progetto
i difetti nelle prove sono stati trovati dai sabotaggi, non dalle revisioni.
Qui si verifica che i due casi nuovi mordano davvero.

**File:**
- Modifica: `assets/shaders/scene.fs` (temporanea, da annullare)
- Modifica: `tools/prove/normalmap.c` (il resoconto)

- [ ] **Passo 1: sabotaggio A — rovesciare la bitangente**

In `SurfaceNormal()`, dopo `b = normalize(b);` inserire `b = -b;`. Poi:

```bash
make prove 2>&1 | grep -A1 "bitangente"
```

Atteso: `bitangente: dalle UV, non dalla w   atteso 129, letto  78  FALLITO`.

Se **passa**, il caso 6a non morde e va ripensato prima di chiudere.

- [ ] **Passo 2: annullare il sabotaggio A**

```bash
git checkout assets/shaders/scene.fs
```

- [ ] **Passo 3: sabotaggio B — inchiodare la tangente a +X**

In `SurfaceNormal()`, subito dopo il calcolo di `t` e `b`, inserire
`t = vec3(1.0, 0.0, 0.0); b = cross(n, t);`. Poi:

```bash
make prove 2>&1 | grep "tangente:"
```

Atteso: `tangente: dalle UV, non inchiodata a +X  atteso 129, letto 104  FALLITO`.

- [ ] **Passo 4: annullare il sabotaggio B e verificare il verde**

```bash
git checkout assets/shaders/scene.fs
make prove 2>&1 | tail -5
```

Atteso: `==> prove passate`.

- [ ] **Passo 5: scrivere il resoconto nella prova**

In coda all'intestazione di `tools/prove/normalmap.c`, dopo la nota del compito
1:

```c
/* Sabotaggi eseguiti il 2026-09-09, a prova verde:
 *   b = -b                            -> 6a legge  78 invece di 129, FALLITO
 *   t = (1,0,0); b = cross(n,t)       -> 6b legge 104 invece di 129, FALLITO
 * I casi mordono. Chi tocca questa prova li rifaccia. */
```

- [ ] **Passo 6: commit**

```bash
git add tools/prove/normalmap.c
git commit -m "I due casi nuovi mordono, verificato sabotandoli

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 4: il rumore sui triangoli sotto il pixel

Le derivate sono per quad di 2×2 frammenti: su un triangolo grande come mezzo
pixel la terna può diventare rumorosa da un fotogramma all'altro. Colpisce solo
dove c'è una normal map **vera** e il triangolo è minuscolo — i prop Poly Haven
a distanza — perché sui materiali con la normale piatta vale
`mat3(t,b,n) * (0,0,1) == n` qualunque sia la terna.

**File:**
- Modifica: `docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md`

- [ ] **Passo 1: costruire il binario strumentato**

Copia dei sorgenti **fuori dal repo**, `GS_PLAY` forzato in `GameInit()` **e**
in `GameNewWorld()`. Al menu i modelli sono già caricati, perché `GameInit()`
chiama `GameNewWorld()`. L'uscita del registro su pipe è bufferizzata: va
rediretta su file, o si legge una schermata vuota.

- [ ] **Passo 2: catturare due inquadrature lontane, prima e dopo**

Due punti di vista con prop Poly Haven a 150-260 m — erba, cespugli, massi,
sottobosco. Per ciascuno, un fotogramma con `HEAD~2` (prima del compito 2) e
uno con il ramo attuale, dalla **stessa** posizione e imbardata.

- [ ] **Passo 3: confrontare a pixel e guardare in movimento**

Il confronto statico dice se il rilievo è cambiato; il rumore però è temporale,
quindi va anche guardato **in movimento** — la camera che avanza lentamente
verso il gruppo di prop. Uno sfarfallio si vede muovendosi, non su un
fotogramma fermo.

- [ ] **Passo 4: scrivere l'esito nella spec**

Nella sezione *Il rischio dichiarato: i triangoli sotto il pixel*, in coda,
aggiungere il risultato con la data: cosa si è guardato, da quali due punti, e
se si è visto o no. Se **non** si vede, scrivere che la mitigazione resta non
scritta e perché. Se **si vede**, non scrivere la mitigazione qui: fermarsi,
riferire, e trattarla come una domanda di design nuova — la soglia andrebbe
tarata contro una geometria che sta per essere sostituita.

- [ ] **Passo 5: commit**

```bash
git add docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md
git commit -m "Il rumore sui triangoli sotto il pixel, guardato

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 5: la misura del costo

Le tangenti si pagavano una volta al caricamento; le derivate si pagano per
frammento. Il passaggio d'ombra **non** entra nel conto: le due uscite
anticipate di `main()` — `depthOnly == 1 && alphaCut <= 0.0` e `depthOnly == 1`
— stanno entrambe prima della riga che costruisce la normale, quindi
`SurfaceNormal()` non viene mai chiamata nelle cascate. Si misura il **solo
passaggio principale**.

**File:**
- Modifica: `docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md`

- [ ] **Passo 1: il rumore di fondo**

Due giri del binario strumentato **non modificato** (sorgenti a `HEAD~2` o
precedenti al compito 2), stesso percorso: `GS_PLAY` forzato, `g->player.yaw`
che ruota per campionare tutte le direzioni, 75 secondi.

Registrare i due tempi del passaggio principale e la loro differenza in
percentuale.

- [ ] **Passo 2: dichiarare la soglia nella spec, PRIMA di misurare il dopo**

Nella sezione *La misura del costo*, passo 2, sostituire «+5% ... da correggere
verso l'alto se il rumore del passo 1 lo impone» con il numero definitivo e il
rumore misurato che lo giustifica. Committare **questo** prima di eseguire il
passo 3.

```bash
git add docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md
git commit -m "La soglia del costo, dichiarata prima di misurare

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

Il commit separato non è pignoleria: è ciò che rende la soglia una previsione
invece di una descrizione. Con un commit solo non si distinguerebbe più.

- [ ] **Passo 3: misurare il dopo**

Stesso banco, stesso percorso, sorgenti del ramo attuale.

- [ ] **Passo 4: scrivere il numero nella spec, qualunque sia**

Tempo prima, tempo dopo, differenza in percentuale, e se rispetta la soglia
dichiarata al passo 2.

- [ ] **Passo 5: commit**

```bash
git add docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md
git commit -m "Il costo delle derivate, misurato

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

- [ ] **Passo 6: il bivio**

Se la differenza **rispetta** la soglia: il compito 5b si salta, e si va al 6.

Se la **sfora**: fermarsi e riferire prima di scrivere il 5b. La rete è
progettata ma la sua necessità è una notizia, non una formalità.

---

## Compito 5b: l'interruttore per lotto — SOLO se il compito 5 sfora

Non si esegue se la misura rispetta la soglia. Accende le derivate solo dove
servono — le mesh animate — e lascia tutto il resto alle tangenti interpolate.

**Una scoperta che lo accorcia:** i personaggi **non passano dall'instancing**.
I lotti di `src/instancing.c` sono i prop, e nessun prop è animato; giocatore e
NPC si disegnano dal percorso non instanziato, in `src/game.c:574-579`. Quindi
`instancing.c` non va toccato: il programma instanziato non vedrà mai il valore
acceso, e il valore di riposo `0` gli basta.

**File:**
- Modifica: `assets/shaders/scene.fs`
- Modifica: `src/light.c` (accanto a `locProjMode`, riga 111) e `src/light.h`
- Modifica: `src/game.c:574-579`

**Interfacce:**
- Produce: `void LightSetTanDeriv(int on);` — `0` usa `fragTangent`, `1` usa le
  derivate. Carica l'uniform su tutti i programmi, come fa `LightFrame()`.

- [ ] **Passo 1: l'uniform nello shader**

Accanto a `projMode` in `assets/shaders/scene.fs`:

```glsl
/* 0 = tangenti del vertice, 1 = derivate di schermo. Lo accende chi disegna i
 * personaggi: solo li' la tangente del vertice e' ferma alla posa di riposo,
 * perche' UpdateModelAnimation() non la aggiorna. Altrove le derivate
 * costerebbero per frammento senza dare niente. */
uniform int tanDeriv;
```

E in `SurfaceNormal()`, come prima riga dopo `vec3 n = normalize(fragNormal);`:

```glsl
    if (tanDeriv == 0) {
        if (dot(fragTangent.xyz, fragTangent.xyz) < 1e-8) return n;
        vec3 tv = fragTangent.xyz - n * dot(n, fragTangent.xyz);
        if (dot(tv, tv) < 1e-8) return n;
        tv = normalize(tv);
        vec3 bv = cross(n, tv) * fragTangent.w;
        vec3 tsv = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
        return normalize(mat3(tv, bv, n) * tsv);
    }
```

- [ ] **Passo 2: la location e il setter in `light.c`**

Accanto a `locProjMode[p]` (riga 111), nello stesso ciclo su `PROG_COUNT`:

```c
        locTanDeriv[p] = GetShaderLocation(gProg[p], "tanDeriv");
```

con la sua dichiarazione accanto alle altre `static int loc...[PROG_COUNT];`.
Poi, accanto a `LightFrame()`:

```c
/* Le derivate di schermo al posto delle tangenti del vertice. Si accende
 * intorno ai personaggi e si rispegne subito dopo: come la soglia dell'alfa
 * nei lotti, uno stato lasciato acceso lo paga chi viene dopo - qui in
 * frammenti, su tutta la scena. */
void LightSetTanDeriv(int on)
{
    if (!gReady) return;
    for (int p = 0; p < PROG_COUNT; p++) {
        if (gProg[p].id == 0 || locTanDeriv[p] == -1) continue;
        SetShaderValue(gProg[p], locTanDeriv[p], &on, SHADER_UNIFORM_INT);
    }
}
```

E la dichiarazione in `src/light.h`, accanto a `LightFrame()`:

```c
void LightSetTanDeriv(int on);
```

- [ ] **Passo 3: accenderlo intorno ai personaggi**

In `src/game.c`, attorno alle righe 574-579. Il passaggio d'ombra alle righe
563-564 **non** va toccato: `SurfaceNormal()` non viene mai chiamata con
`depthOnly == 1`, quindi lì l'interruttore non farebbe niente.

```c
        LightSetTanDeriv(1);
        EntitiesDraw(g->ents, &g->world, g->cam, tint);
        ...
        PlayerDraw(&g->player, tint);
        LightSetTanDeriv(0);
```

Il rispristino a `0` non è opzionale: senza, tutto ciò che si disegna dopo i
personaggi nel fotogramma pagherebbe le derivate.

- [ ] **Passo 4: la prova deve restare verde**

```bash
make prove 2>&1 | tail -5
```

`normalmap` disegna con `DrawMesh()` e non chiama `LightSetTanDeriv()`, quindi
legge il valore di riposo `0` — cioè il percorso delle **tangenti**, e i due
casi nuovi torneranno **rossi**. È corretto e atteso.

Vanno quindi aggiunte due righe alla sezione 6 di `tools/prove/normalmap.c`,
prima dei due `Near()`:

```c
    /* Il percorso da provare qui e' quello delle derivate, che in gioco si
     * accende solo sui personaggi. Senza questa riga la prova misurerebbe il
     * ripiego e i due casi sotto sarebbero rossi per costruzione. */
    LightSetTanDeriv(1);
```

e, in coda alla sezione, un caso che verifica **anche il ripiego**, perché ora
esistono due percorsi e quello spento non lo prova nessuno:

```c
    /* Con l'interruttore spento si torna alle tangenti del vertice, e la w
     * dichiarata da Quadrato() rovescia la bitangente: 78, non 129. E' il
     * valore giusto per QUEL percorso, ed e' cio' che distingue i due. */
    LightSetTanDeriv(0);
    UnloadTexture(mat.maps[MATERIAL_MAP_NORMAL].texture);
    mat.maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(128, 191, 238);
    Near("interruttore spento: torna alle tangenti", Centro(rt, q, mat), 78, 3);
```

- [ ] **Passo 5: rimisurare**

Stesso banco, stesso percorso del compito 5. Il numero va nella spec accanto
agli altri, e deve ora rispettare la soglia — se non la rispetta nemmeno così,
il costo non veniva dalle derivate e l'ipotesi va rifatta.

- [ ] **Passo 6: commit**

```bash
git add assets/shaders/scene.fs src/light.c src/light.h src/game.c \
        tools/prove/normalmap.c \
        docs/superpowers/specs/2026-09-09-tangenti-da-derivate-design.md
git commit -m "Le derivate solo sui personaggi, che e' dove servono

La misura ha sforato la soglia dichiarata. L'interruttore viaggia sui
programmi come gli altri stati di light.c e si rispegne subito dopo i
personaggi: uno stato lasciato acceso lo paga chi viene dopo, qui in
frammenti su tutta la scena.

L'instancing non e' toccato: nessun prop e' animato, quindi il programma
instanziato non vede mai il valore acceso.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 6: i documenti

**File:**
- Modifica: `docs/01-architettura.md`, sezione *Normal map* (righe 132-172) e
  sezione *Le prove* (dalla riga 574)
- Modifica: `docs/06-stato-e-prossimi-passi.md`, la domanda F

- [ ] **Passo 1: `docs/01`, sezione *Normal map***

La sezione descrive oggi la terna dall'attributo e le sue tre trappole.
Riscriverla per dire che la terna viene dalle derivate, mantenendo le tre
trappole che restano vere — sono ancora il motivo per cui `BuildTangents()`
esiste, e `BuildTangents()` non è stato tolto (è fuori ambito).

In particolare la riga 169, che oggi dice che «un personaggio animato con una
normal map avrà quindi tangenti [ferme]», va riscritta: non è più vero, ed è il
punto di questo lavoro.

Aggiungere il limite che resta: le UV degeneri non sono coperte, e i triangoli
sotto il pixel sono il rischio dichiarato.

E il dettaglio che altrimenti sembrera' un difetto nuovo a chi lo incontra:
sulle foglie il `discard` del ritaglio rende il flusso non uniforme dentro il
quad 2x2, quindi le derivate al bordo del ritaglio sono approssimate. E' la
stessa approssimazione che le GPU fanno gia' oggi per scegliere il livello di
mip di `texture()`, con le stesse corsie d'aiuto: non e' una regressione
introdotta da questo lavoro.

- [ ] **Passo 2: `docs/01`, sezione *Le prove***

L'elenco dei controlli di `normalmap` va aggiornato con i due casi nuovi, e con
il motivo per cui i fixture dichiarano tangenti in disaccordo con le proprie
UV — altrimenti sembrano errori.

- [ ] **Passo 3: `docs/06`, la domanda F**

Da «design pronto, **non approvato**» a chiusa, con: il numero della misura, il
verdetto sul rumore, e cosa resta aperto — la rimozione di `BuildTangents()`,
dell'attributo `vertexTangent` e del varying `fragTangent`, con la condizione
che la sblocca (*il percorso nuovo è provato in gioco su un personaggio con
normal map vera*).

Aggiornare anche l'elenco dei documenti collegati con la spec e questo piano, e
la riga della domanda C: le tangenti non la bloccano più.

- [ ] **Passo 4: commit**

```bash
git add docs/01-architettura.md docs/06-stato-e-prossimi-passi.md
git commit -m "I documenti dicono che la terna viene dalle derivate

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Compito 7: chiusura

- [ ] **Passo 1: la verifica completa**

```bash
make && make prove && make valida
```

Atteso: zero avvisi, `==> prove passate`, validazione a posto.

- [ ] **Passo 2: rileggere il diff per intero**

```bash
git diff main...tangenti-derivate
```

Due difetti di correttezza dello shader, in questo progetto, sono stati trovati
**leggendo** e nessuna prova poteva vederli. Vale la pena rifarlo.

- [ ] **Passo 3: riferire**

Il numero della misura, l'esito del rumore, se il 5b è stato eseguito, e cosa
resta aperto. Poi usare `superpowers:finishing-a-development-branch` per
decidere come integrare.
