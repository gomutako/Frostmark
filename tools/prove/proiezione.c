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
