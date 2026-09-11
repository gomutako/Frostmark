/* ============================================================================
 * normalmap.c - La normal map nello shader, e le tangenti che la reggono.
 *
 * Due cose da dimostrare, e la prima conta quanto la seconda:
 *   1. con gli asset senza normal map il rendering NON cambia;
 *   2. con una normal map la normale si piega, e nel verso giusto.
 *
 * I valori attesi sono calcolati a mano prima di guardarli, altrimenti la
 * prova si limita a fotografare cio' che il codice fa - compresi gli errori.
 * Con sole (0.6, 0.8, 0), albedo 100/255, ombre spente e la formula
 * AMBIENT + SUN * max(dot(n, sole), 0):
 *
 *   normale piatta  n = (0, 1, 0)       diff 0.800  luce 1.130  ->  113
 *   piegata di +30  n = (0.5, .866, 0)  diff 0.993  luce 1.294  ->  129
 *   piegata di -30  n = (-.5, .866, 0)  diff 0.393  luce 0.784  ->   78
 *
 * Include light.c perche' gShader e le location sono static.
 * ========================================================================== */

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

/* Sabotaggi eseguiti il 2026-09-09, a prova verde:
 *   b = -b                            -> 6a legge  79 invece di 129, FALLITO
 *   t = (1,0,0); b = cross(n,t)       -> 6b legge 104 invece di 129, FALLITO
 * I casi mordono. Chi tocca questa prova li rifaccia.
 * Il conto a mano per 6a rovesciato da' 78 e la GPU legge 79: e' lo stesso
 * arrotondamento che si vede da sempre sul caso "piegata via dal sole", e sta
 * dentro la tolleranza 3 di Near(). Il sabotaggio non da' un numero diverso da
 * quello previsto, lo da' con un livello di scarto. */
#include "../../src/light.c"
#include "prova.h"

#include <string.h>
#include <unistd.h>

static int HaUniform(const char *nome)
{
    return GetShaderLocation(gShader, nome) != -1;
}

/* Quadrato nel piano XZ, normale +Y, tangente +X dichiarata a mano: cosi' la
 * terna e' nota e il valore atteso si calcola con carta e penna, senza
 * dipendere da come si orientano le UV. */
static Mesh Quadrato(void)
{
    static float v[18]  = { -2,0,-2,  -2,0,2,   2,0,2,
                            -2,0,-2,   2,0,2,   2,0,-2 };
    static float n[18]  = { 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0 };
    static float uv[12] = { 0,0, 0,1, 1,1, 0,0, 1,1, 1,0 };
    static float tg[24] = { 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1 };

    Mesh m = { 0 };
    m.vertexCount = 6;
    m.triangleCount = 2;
    m.vertices = v; m.normals = n; m.texcoords = uv; m.tangents = tg;
    UploadMesh(&m, false);
    return m;
}

/* Lo stesso quadrato ma INDICIZZATO e senza tangenti: quattro vertici condivisi
 * da due triangoli, com'e' fatta una mesh glTF vera. E' il caso su cui
 * GenMeshTangents() di raylib sbaglia, perche' ignora gli indici. La u cresce
 * lungo +X e la v lungo +Z, quindi la tangente attesa e' (1,0,0) con verso -1. */
static Mesh QuadratoIndicizzato(void)
{
    static float v[12]  = { -2,0,-2,  -2,0,2,  2,0,2,  2,0,-2 };
    static float n[12]  = { 0,1,0, 0,1,0, 0,1,0, 0,1,0 };
    static float uv[8]  = { 0,0,  0,1,  1,1,  1,0 };
    static unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };

    Mesh m = { 0 };
    m.vertexCount = 4;
    m.triangleCount = 2;
    m.vertices = v; m.normals = n; m.texcoords = uv; m.indices = idx;
    UploadMesh(&m, false);
    return m;
}

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

/* Un pixel di normal map, in byte. (128,128,255) vale (0,0,1): non piegare. */
static Texture2D PixelNormale(int r, int g, int b)
{
    Image im = GenImageColor(1, 1, (Color){ (unsigned char)r, (unsigned char)g,
                                            (unsigned char)b, 255 });
    Texture2D t = LoadTextureFromImage(im);
    UnloadImage(im);
    return t;
}

/* Disegna il quadrato dall'alto e restituisce il canale rosso al centro. */
static int Centro(RenderTexture2D rt, Mesh q, Material mat)
{
    Camera3D cam = { 0 };
    cam.position   = (Vector3){ 0.0f, 5.0f, 0.0f };
    cam.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up         = (Vector3){ 0.0f, 0.0f, 1.0f };
    cam.fovy       = 2.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(cam);
            rlDisableBackfaceCulling();
            DrawMesh(q, mat, MatrixIdentity());
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    Color c = GetImageColor(img, rt.texture.width / 2, rt.texture.height / 2);
    UnloadImage(img);
    return c.r;
}

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova normal map");
    if (!IsWindowReady()) {
        printf("niente contesto GL: prova saltata\n");
        return PROVA_SALTATA;
    }

    /* --- 1. lo shader compila e le uniform si risolvono tutte ------------- */
    Ok("LightInit(): shader compilato", LightInit());
    if (!LightReady()) { CloseWindow(); return 1; }

    Ok("uniform texture0 (albedo)",     HaUniform("texture0"));
    Ok("uniform texture2 (normal map)", HaUniform("texture2"));
    Ok("uniform lightDir",  HaUniform("lightDir"));
    Ok("uniform sunAmount", HaUniform("sunAmount"));
    Ok("uniform depthOnly", HaUniform("depthOnly"));
    Ok("uniform shadowOn",  HaUniform("shadowOn"));
    Ok("uniform shadowRes", HaUniform("shadowRes"));
    Ok("uniform lightVP0 / lightVP1",
       HaUniform("lightVP0") && HaUniform("lightVP1"));
    Ok("uniform shadowMap0 / shadowMap1",
       HaUniform("shadowMap0") && HaUniform("shadowMap1"));
    Ok("uniform viewPos / splitDist",
       HaUniform("viewPos") && HaUniform("splitDist"));
    Ok("attributo vertexTangent legato da raylib",
       gShader.locs[SHADER_LOC_VERTEX_TANGENT] != -1);

    /* --- 2. chi non ha normal map ne riceve una piatta -------------------- */
    Material nudo = LoadMaterialDefault();
    Ok("materiale nudo: nessuna normal map prima",
       nudo.maps[MATERIAL_MAP_NORMAL].texture.id == 0);
    LightApplyToMaterial(&nudo);
    Ok("materiale nudo: normale piatta dopo",
       nudo.maps[MATERIAL_MAP_NORMAL].texture.id != 0);

    /* --- 3. mesh con normal map ma senza tangenti: si calcolano ----------- */
    Model piano = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    Ok("GenMeshPlane non porta tangenti", piano.meshes[0].tangents == NULL);
    piano.materials[0].maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(128, 128, 255);
    LightApplyToModel(&piano);
    Ok("tangenti calcolate al caricamento", piano.meshes[0].tangents != NULL);

    /* --- 4. i numeri ------------------------------------------------------ */
    LightSetSun((Vector3){ 0.6f, 0.8f, 0.0f }, 1.0f);
    Camera3D nulla = { 0 };
    LightFrame(nulla);

    /* Ombre spente a mano: qui si misura la normale, non la mappa di
     * profondita', e con le ombre accese il fattore entrerebbe nel conto. */
    int zero = 0;
    SetShaderValue(gShader, GetShaderLocation(gShader, "shadowOn"),  &zero, SHADER_UNIFORM_INT);
    SetShaderValue(gShader, GetShaderLocation(gShader, "depthOnly"), &zero, SHADER_UNIFORM_INT);

    RenderTexture2D rt = LoadRenderTexture(64, 64);
    Mesh q = Quadrato();
    Material mat = LoadMaterialDefault();
    mat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 100, 100, 100, 255 };
    LightApplyToMaterial(&mat);                  /* installa la normale piatta */

    Near("normale piatta: rendering invariato", Centro(rt, q, mat), 113, 3);

    UnloadTexture(mat.maps[MATERIAL_MAP_NORMAL].texture);
    mat.maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(191, 128, 238);
    Near("piegata verso il sole: piu' chiara", Centro(rt, q, mat), 129, 3);

    UnloadTexture(mat.maps[MATERIAL_MAP_NORMAL].texture);
    mat.maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(64, 128, 238);
    Near("piegata via dal sole: piu' scura", Centro(rt, q, mat), 78, 3);

    /* --- 5. mesh indicizzata: il caso che GenMeshTangents() sbaglia -------- */
    Model iq = LoadModelFromMesh(QuadratoIndicizzato());
    Ok("quadrato indicizzato: nessuna tangente prima",
       iq.meshes[0].tangents == NULL);
    iq.materials[0].maps[MATERIAL_MAP_NORMAL].texture = PixelNormale(191, 128, 238);
    LightApplyToModel(&iq);
    Ok("quadrato indicizzato: tangenti calcolate", iq.meshes[0].tangents != NULL);

    if (iq.meshes[0].tangents != NULL) {
        const float *t = iq.meshes[0].tangents;
        int buona = 1;
        for (int v = 0; v < 4; v++)
            buona = buona && fabsf(t[v*4+0] - 1.0f) < 1e-4f
                          && fabsf(t[v*4+1])        < 1e-4f
                          && fabsf(t[v*4+2])        < 1e-4f
                          && fabsf(t[v*4+3] + 1.0f) < 1e-4f;
        printf("  tangente al vertice 0: (%.3f, %.3f, %.3f) verso %.0f\n",
               (double)t[0], (double)t[1], (double)t[2], (double)t[3]);
        Ok("tangenti indicizzate: (1,0,0) con verso -1", buona);
    }

    iq.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 100, 100, 100, 255 };
    Near("indicizzato, piegato verso il sole",
         Centro(rt, iq.meshes[0], iq.materials[0]), 129, 3);

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

    CloseWindow();
    return ProveEsito();
}
