/* ============================================================================
 * alfa.c - Il ritaglio dell'alfa, compreso il passaggio di profondita'.
 *
 * Nel catalogo Poly Haven ogni pianta e' fatta di ritagli su quadrati: le
 * foglie stanno nell'alfa della texture, e il quadrato va fatto sparire dove
 * l'alfa e' bassa. Tre cose da dimostrare, e la terza e' quella che si
 * dimentica:
 *
 *   1. una texture OPACA non attiva niente, e il rendering resta identico;
 *   2. una texture con alfa fa sparire i frammenti trasparenti;
 *   3. spariscono ANCHE nel passaggio di profondita', o l'ombra di una fronda
 *      sarebbe un rettangolo.
 *
 * Il passaggio di profondita' si prova facile: con depthOnly acceso lo shader
 * scrive bianco pieno, quindi cio' che sopravvive e' bianco e cio' che viene
 * scartato resta il nero dello sfondo. Si contano i pixel bianchi.
 * ========================================================================== */
#include "../../src/light.c"
#include "../../src/instancing.c"
#include "prova.h"

#include <string.h>
#include <unistd.h>

/* Quadrato nel piano XZ, normale +Y, con UV che coprono tutta la texture. */
static Mesh Quadrato(void)
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

/* Una texture 2x2: la meta' sinistra opaca, la destra trasparente. Se 'conAlfa'
 * e' falso si costruisce la stessa immagine senza canale alfa, cosi' il
 * rilevamento non deve accendere niente. */
static Texture2D MezzaTrasparente(bool conAlfa)
{
    Image im = GenImageColor(2, 2, WHITE);
    ImageDrawPixel(&im, 1, 0, (Color){ 255, 255, 255, 0 });
    ImageDrawPixel(&im, 1, 1, (Color){ 255, 255, 255, 0 });
    ImageFormat(&im, conAlfa ? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
                             : PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    Texture2D t = LoadTextureFromImage(im);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(im);
    return t;
}

/* Quanti pixel non sono lo sfondo nero. */
static int PixelAccesi(Image im)
{
    int n = 0;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++)
            if (GetImageColor(im, x, y).r > 8) n++;
    return n;
}

static int Rendi(RenderTexture2D rt, InstBatch *b)
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
            InstAdd(b, (Vector3){ 0, 0, 0 }, 0.0f, (Vector3){ 1, 1, 1 });
            InstFlush(b);
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    int n = PixelAccesi(img);
    UnloadImage(img);
    return n;
}

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova alfa");
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

    RenderTexture2D rt = LoadRenderTexture(120, 120);
    Mesh q = Quadrato();

    /* --- 1. texture opaca: il ritaglio non si accende --------------------- */
    Material opaco = LoadMaterialDefault();
    LightApplyToMaterial(&opaco);
    opaco.maps[MATERIAL_MAP_DIFFUSE].texture = MezzaTrasparente(false);
    Ok("texture senza alfa: soglia a zero",
       LightAlphaCutFor(opaco) == 0.0f);
    InstBatch *bo = InstCreate(q, opaco);
    Ok("lotto opaco creato", bo != NULL);
    int nOpaco = Rendi(rt, bo);

    /* --- 2. texture con alfa: il ritaglio si accende da se' -------------- */
    Material ritaglio = LoadMaterialDefault();
    LightApplyToMaterial(&ritaglio);
    ritaglio.maps[MATERIAL_MAP_DIFFUSE].texture = MezzaTrasparente(true);
    Ok("texture con alfa: soglia accesa",
       LightAlphaCutFor(ritaglio) > 0.0f);
    InstBatch *br = InstCreate(q, ritaglio);
    Ok("lotto con ritaglio creato", br != NULL);
    int nRitaglio = Rendi(rt, br);

    printf("  pixel accesi: opaco %d, ritagliato %d\n", nOpaco, nRitaglio);
    Ok("l'opaco copre tutto il quadrato", nOpaco > 4000);
    /* Meta' esatta, con qualche pixel di tolleranza sul bordo del texel. */
    Near("il ritagliato ne copre la meta'", nRitaglio, nOpaco / 2, nOpaco / 20);

    /* --- 3. e vale anche nel passaggio di profondita' --------------------- */
    int uno = 1;
    for (int p = 0; p < PROG_COUNT; p++)
        if (gProg[p].id != 0)
            SetShaderValue(gProg[p], GetShaderLocation(gProg[p], "depthOnly"),
                           &uno, SHADER_UNIFORM_INT);

    int dOpaco = Rendi(rt, bo);
    int dRitaglio = Rendi(rt, br);
    printf("  profondita': opaco %d, ritagliato %d\n", dOpaco, dRitaglio);
    Ok("profondita': l'opaco scrive tutto", dOpaco > 4000);
    Near("profondita': il ritagliato scrive la meta'",
         dRitaglio, dOpaco / 2, dOpaco / 20);

    InstFree(bo);
    InstFree(br);
    CloseWindow();
    return ProveEsito();
}
