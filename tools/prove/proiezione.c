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

/* Stessa geometria del quadrato piatto, ma con le normali dichiarate lungo +X.
 * Serve a esercitare l'altro ramo di AsseDominante(): la normale la decide il
 * vertice, non la posizione, e da qui si vede la faccia comunque. Con l'asse X
 * la U segue la z, quindi il motivo deve correre lungo le COLONNE invece che
 * lungo le righe - lo specchio esatto del caso +Y. */
static Mesh QuadratoNormaleX(void)
{
    static float v[18]  = { -2,0,-2,  -2,0,2,   2,0,2,
                            -2,0,-2,   2,0,2,   2,0,-2 };
    static float n[18]  = { 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0 };
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
    cam.fovy       = 5.2f;
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

/* Quante volte il rosso fa uno sbalzo brusco fra due pixel vicini. In valore
 * assoluto e non solo in discesa: il verso dipende da come e' orientata la
 * camera - qui lo schermo-x segue il mondo -x - e la prova deve misurare il
 * motivo, non l'orientamento dell'inquadratura.
 *
 * Contano solo le coppie interamente sul quadrato: al bordo la transizione col
 * nero e' uno sbalzo pieno e conterebbe come una ripetizione che non c'e'. La
 * rampa scrive verde 40 dappertutto, quindi il verde dice se il pixel e' sul
 * quadrato; il rosso no, perche' sul lato basso della rampa vale quasi zero. */
static bool SulQuadrato(Color c) { return c.g > 20; }

static int SbalziInRiga(Image im, int y)
{
    int n = 0;
    for (int x = 1; x < im.width; x++) {
        Color a = GetImageColor(im, x - 1, y), b = GetImageColor(im, x, y);
        if (!SulQuadrato(a) || !SulQuadrato(b)) continue;
        if (abs((int)a.r - (int)b.r) > 100) n++;
    }
    return n;
}

static int SbalziInColonna(Image im, int x)
{
    int n = 0;
    for (int y = 1; y < im.height; y++) {
        Color a = GetImageColor(im, x, y - 1), b = GetImageColor(im, x, y);
        if (!SulQuadrato(a) || !SulQuadrato(b)) continue;
        if (abs((int)a.r - (int)b.r) > 100) n++;
    }
    return n;
}

/* Il centro (in pixel) dei pixel che stanno sul quadrato: il centro del CORPO
 * dell'oggetto sullo schermo, qualunque sia il verso in cui la camera lo
 * disegna. Serve al confronto di fase fra due istanze, che non deve
 * indovinare da che parte si sposta lo schermo quando l'istanza si sposta nel
 * mondo. */
static void CentroQuadrato(Image im, int *cx, int *cy)
{
    long sx = 0, sy = 0, n = 0;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++) {
            if (!SulQuadrato(GetImageColor(im, x, y))) continue;
            sx += x; sy += y; n++;
        }
    *cx = (n > 0) ? (int)(sx / n) : im.width / 2;
    *cy = (n > 0) ? (int)(sy / n) : im.height / 2;
}

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
    int sbalzi0 = SbalziInRiga(im0, RT / 2);
    Ok("modo 0: la mesh usa le sue UV, una rampa sola", sbalzi0 == 0);

    /* --- 2. modo 1: proiezione con passo di 1 m sul quadrato da 4 m ------ */
    InstProjection(b, 1, 1.0f);
    Image im1 = Rendi(rt, b, 0.0f);
    int sbalzi1 = SbalziInRiga(im1, RT / 2);
    printf("  sbalzi in riga: modo 0 = %d, modo 1 = %d\n", sbalzi0, sbalzi1);
    Ok("modo 1: il passo di 1 m ripete la texture sul quadrato di 4 m",
       sbalzi1 >= 3);

    /* Il quadrato ha normale +Y, quindi l'asse dominante e' Y e la U segue
     * la x del mondo: lungo la z - le colonne dell'immagine - non deve
     * cambiare niente. */
    Ok("modo 1: sulla faccia +Y la U segue x e non z",
       SbalziInColonna(im1, RT / 2) == 0);

    /* --- 2b. lo stesso quadrato, ma con la normale dichiarata lungo +X --- */
    /* Il sole verso +Y non illumina piu' niente su questa faccia - il colore
     * si schiaccia sull'ambiente e SulQuadrato() (che guarda il verde dopo la
     * luce) non distinguerebbe piu' il quadrato dallo sfondo. Il sole si
     * sposta solo per questo blocco e torna al suo posto subito dopo, perche'
     * i controlli 3 e 4 contano sull'illuminazione originale sulla faccia +Y. */
    LightSetSun((Vector3){ 1.0f, 0.0f, 0.0f }, 1.0f);
    LightFrame(nulla);

    Mesh qx = QuadratoNormaleX();
    InstBatch *bx = InstCreate(qx, mat);
    Ok("lotto X creato", bx != NULL);
    if (bx != NULL) {
        InstProjection(bx, 1, 1.0f);
        Image imX = Rendi(rt, bx, 0.0f);
        int sbalziRigaX = SbalziInRiga(imX, RT / 2);
        int sbalziColX  = SbalziInColonna(imX, RT / 2);
        printf("  normale +X: sbalzi in riga %d, in colonna %d\n",
               sbalziRigaX, sbalziColX);
        Ok("modo 1: sulla faccia +X la U segue z e non x",
           sbalziColX >= 3 && sbalziRigaX == 0);
        UnloadImage(imX);
        InstFree(bx);
    }

    LightSetSun((Vector3){ 0.0f, 1.0f, 0.0f }, 1.0f);
    LightFrame(nulla);

    /* --- 3. la proiezione e' in spazio oggetto --------------------------- */
    /* Ruotando l'istanza di 90 gradi il motivo deve girare CON l'oggetto: gli
     * sbalzi passano dalle righe alle colonne. Proiettando in coordinate di
     * mondo resterebbero nelle righe, perche' la texture scivolerebbe sotto
     * l'oggetto invece di essere incollata. */
    Image im90 = Rendi(rt, b, 90.0f);
    int sbalziRiga90 = SbalziInRiga(im90, RT / 2);
    int sbalziCol90  = SbalziInColonna(im90, RT / 2);
    printf("  ruotato di 90 gradi: sbalzi in riga %d, in colonna %d\n",
           sbalziRiga90, sbalziCol90);
    Ok("spazio oggetto: a 90 gradi il motivo gira con l'oggetto",
       sbalziCol90 >= 3 && sbalziRiga90 == 0);

    /* --- 4. due istanze in posizioni diverse ricevono fasi diverse ------- */
    /* Senza sfalsamento, trenta case avrebbero la venatura identica nello
     * stesso punto del proprio corpo. Si trova il CENTROIDE del quadrato in
     * ciascuna immagine, non un punto calcolato a mano: cosi' la prova non
     * deve indovinare da che parte lo schermo si sposta quando l'istanza si
     * sposta nel mondo - che dipende dal verso della camera, non dal motore. */
    Image imP = Rendi(rt, b, 0.0f);              /* istanza all'origine */
    int cxP, cyP;
    CentroQuadrato(imP, &cxP, &cyP);

    /* Non al centro del corpo: senza sfalsamento il centro da' u = 0, cioe'
     * esattamente la cucitura della rampa, e un arrotondamento sub-pixel
     * manderebbe il campione da una parte o dall'altra - la differenza
     * risulterebbe grande anche con lo sfalsamento spento, e il controllo non
     * proverebbe niente. Di fianco al centro la u sta in mezzo a un periodo in
     * tutti e due i casi: 0,65 m senza sfalsamento, 1,15 con.
     *
     * I 15 pixel valgono per entrambe le rese perche' le due istanze sono solo
     * traslate, non ruotate: lo stesso scarto dal loro centroide e' lo stesso
     * punto del loro corpo, comunque sia orientata la camera. */
    static const int SCARTO = 15;
    int rossoOrigine = GetImageColor(imP, cxP + SCARTO, cyP).r;

    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(((Camera3D){ .position = { 0.0f, 5.0f, 0.0f },
                                 .target = { 0.0f, 0.0f, 0.0f },
                                 .up = { 0.0f, 0.0f, 1.0f },
                                 .fovy = 5.2f,
                                 .projection = CAMERA_ORTHOGRAPHIC }));
            rlDisableBackfaceCulling();
            InstBegin(b);
            InstAdd(b, (Vector3){ 0.5f, 0, 0 }, 0.0f, (Vector3){ 1, 1, 1 });
            InstFlush(b);
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();
    Image imQ = LoadImageFromTexture(rt.texture);

    int cxQ, cyQ;
    CentroQuadrato(imQ, &cxQ, &cyQ);
    int rossoSpostato = GetImageColor(imQ, cxQ + SCARTO, cyQ).r;

    printf("  rosso di fianco al centro del corpo: all'origine %d, spostata %d\n",
           rossoOrigine, rossoSpostato);
    Ok("lo sfalsamento cambia la fase fra istanze in posizioni diverse",
       abs(rossoOrigine - rossoSpostato) > 60);

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

    UnloadImage(im0); UnloadImage(im1); UnloadImage(im90);
    UnloadImage(imP); UnloadImage(imQ);
    InstFree(b);

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

    UnloadRenderTexture(rt);
    CloseWindow();
    return ProveEsito();
}
