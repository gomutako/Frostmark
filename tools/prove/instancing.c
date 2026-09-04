/* ============================================================================
 * instancing.c - Lo stesso oggetto, disegnato nei due modi, deve dare gli
 * stessi pixel.
 *
 * E' la prova che conta di tutto il modulo: il disegno a istanze ricostruisce
 * a mano, nel vertex shader, quello che raylib fa con le matrici. Se sbaglia il
 * verso della rotazione la scena esce specchiata; se moltiplica la normale per
 * la scala invece di dividerla, la forma e' giusta ma l'illuminazione no. Sono
 * due errori che a occhio, su un albero solo, non si vedono.
 *
 * Percio' la prova usa scala NON UNIFORME e rotazione NON NULLA: sono i due
 * casi in cui quegli errori diventano visibili. E usa due mesh, una
 * indicizzata e una no, perche' il disegno a istanze prende due strade diverse.
 * ========================================================================== */
/* Si includono ENTRAMBI: instancing.c per il modulo sotto prova, light.c
 * perche' la prova deve spegnere le ombre su tutti e due i programmi e gProg
 * e' static. Inclusi e non collegati, o i simboli si duplicano. */
#include "../../src/light.c"
#include "../../src/instancing.c"
#include "prova.h"

#include <string.h>
#include <unistd.h>

/* Scala non uniforme e rotazione non nulla: le due condizioni che rendono
 * visibile un errore di segno o di inversa trasposta. */
static const Vector3 POS   = { 0.3f, -0.2f, 0.0f };
static const float   YAW   = 35.0f;
static const Vector3 SCALA = { 1.0f, 2.5f, 0.7f };

static Camera3D Vista(void)
{
    Camera3D c = { 0 };
    c.position   = (Vector3){ 4.0f, 3.0f, 4.5f };
    c.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    c.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    c.fovy       = 45.0f;
    c.projection = CAMERA_PERSPECTIVE;
    return c;
}

static Image RendiConLotto(RenderTexture2D rt, InstBatch *b)
{
    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(Vista());
            InstBegin(b);
            InstAdd(b, POS, YAW, SCALA);
            InstFlush(b);
        EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(rt.texture);
}

static Image RendiNormale(RenderTexture2D rt, Mesh m, Material mat)
{
    /* Stesso ordine di DrawModelEx: scala, poi rotazione, poi posizione. */
    Matrix t = MatrixMultiply(MatrixMultiply(
                   MatrixScale(SCALA.x, SCALA.y, SCALA.z),
                   MatrixRotate((Vector3){ 0.0f, 1.0f, 0.0f }, YAW * DEG2RAD)),
                   MatrixTranslate(POS.x, POS.y, POS.z));
    BeginTextureMode(rt);
        ClearBackground(BLACK);
        BeginMode3D(Vista());
            DrawMesh(m, mat, t);
        EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(rt.texture);
}

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

/* Quanti pixel sono accesi: se il lotto non disegnasse NIENTE, il confronto
 * fra due immagini nere passerebbe per il motivo sbagliato. */
static int PixelAccesi(Image im)
{
    int n = 0;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++)
            if (GetImageColor(im, x, y).r > 8) n++;
    return n;
}

/* Quante luminosita' diverse ci sono: su un cubo le facce guardano da parti
 * diverse, quindi devono essere illuminate diversamente. Se ne esce una sola,
 * le normali non vengono trasformate e la prova d'equivalenza passerebbe
 * ugualmente - due immagini sbagliate allo stesso modo. */
static int LivelliDiversi(Image im)
{
    int visto[256] = { 0 }, n = 0;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++) {
            int v = GetImageColor(im, x, y).r;
            if (v > 8 && !visto[v]) { visto[v] = 1; n++; }
        }
    return n;
}

/* 'livelliMin' e' quante luminosita' distinte ci si aspetta di vedere, ed e'
 * misurato, non indovinato: da questa camera il cubo mostra due facce, a 195 e
 * 222 sul canale rosso, mentre la sfera da' un degrade di 161 livelli. Serve a
 * escludere il caso in cui le normali non vengono trasformate affatto: allora
 * l'oggetto esce di un colore piatto, e la prova d'equivalenza passerebbe
 * ugualmente perche' le due immagini sarebbero sbagliate allo stesso modo. */
static void ProvaMesh(const char *nome, Mesh m, RenderTexture2D rt, int livelliMin)
{
    char msg[128];

    Material mat = LoadMaterialDefault();
    mat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 200, 200, 200, 255 };
    LightApplyToMaterial(&mat);

    InstBatch *b = InstCreate(m, mat);
    snprintf(msg, sizeof msg, "%s: il lotto si crea", nome);
    Ok(msg, b != NULL);
    if (b == NULL) return;

    Image ia = RendiConLotto(rt, b);
    Image ib = RendiNormale(rt, m, mat);

    int accesi = PixelAccesi(ib);
    snprintf(msg, sizeof msg, "%s: qualcosa e' stato disegnato", nome);
    Ok(msg, accesi > 200);

    int livelli = LivelliDiversi(ib);
    snprintf(msg, sizeof msg, "%s: le normali sono trasformate", nome);
    Ok(msg, livelli >= livelliMin);

    int diversi = PixelDiversi(ia, ib, 2);
    printf("  %s: %d pixel diversi su %d, %d accesi, %d livelli\n",
           nome, diversi, ia.width * ia.height, accesi, livelli);
    snprintf(msg, sizeof msg, "%s: instanziato = non instanziato", nome);
    Ok(msg, diversi == 0);

    UnloadImage(ia);
    UnloadImage(ib);
    InstFree(b);
}

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova instancing");
    if (!IsWindowReady()) {
        printf("niente contesto GL: prova saltata\n");
        return PROVA_SALTATA;
    }

    Ok("LightInit()", LightInit());
    if (!LightReady()) { CloseWindow(); return 1; }
    Ok("lo shader instanziato c'e'", LightInstShader().id != 0);

    LightSetSun((Vector3){ 0.45f, 0.78f, 0.44f }, 1.0f);
    Camera3D nulla = { 0 };
    LightFrame(nulla);

    /* Ombre spente su ENTRAMBI i programmi: qui si misura la geometria, e una
     * mappa d'ombra mezza inizializzata darebbe rumore diverso ai due. */
    int zero = 0;
    for (int p = 0; p < PROG_COUNT; p++) {
        if (gProg[p].id == 0) continue;
        SetShaderValue(gProg[p], GetShaderLocation(gProg[p], "shadowOn"),  &zero, SHADER_UNIFORM_INT);
        SetShaderValue(gProg[p], GetShaderLocation(gProg[p], "depthOnly"), &zero, SHADER_UNIFORM_INT);
    }

    RenderTexture2D rt = LoadRenderTexture(160, 160);

    /* Due mesh per due strade: il disegno a istanze usa
     * rlDrawVertexArrayElementsInstanced() se la mesh e' indicizzata e
     * rlDrawVertexArrayInstanced() se non lo e', e vanno provate entrambe.
     *
     * Quale sia quale e' controintuitivo, e infatti la prima versione di
     * questa prova lo dava al contrario: in raylib 5.5 il CUBO e' indicizzato
     * (24 vertici, 12 triangoli) e la SFERA no (1152 vertici sciolti). Percio'
     * non si asserisce chi e' cosa, si controlla che ci sia una per strada. */
    Mesh cubo  = GenMeshCube(1.0f, 1.0f, 1.0f);
    Mesh sfera = GenMeshSphere(0.7f, 12, 16);
    Ok("una mesh indicizzata e una no, per provare le due strade",
       (cubo.indices != NULL) != (sfera.indices != NULL));
    printf("  cubo %s, sfera %s\n",
           cubo.indices  ? "indicizzato" : "non indicizzato",
           sfera.indices ? "indicizzata" : "non indicizzata");

    /* LA SFERA NON E' RIDONDANTE. Verificato sabotando lo shader di proposito:
     * invertendo il verso della rotazione le prendono entrambe (790 e 2605
     * pixel diversi), ma moltiplicando la normale per la scala invece di
     * dividerla il CUBO NON SE NE ACCORGE - zero pixel diversi - mentre la
     * sfera se ne accorge con 2782.
     *
     * Il motivo: le normali del cubo sono versori sugli assi, e (1,0,0) diviso
     * o moltiplicato per (1, 2.5, 0.7) resta (1,0,0) una volta normalizzato.
     * Solo una normale con componenti su piu' assi distingue i due casi. Con il
     * solo cubo questa prova darebbe falsa sicurezza proprio sull'errore piu'
     * facile da fare. */
    ProvaMesh("cubo",  cubo,  rt, 2);
    ProvaMesh("sfera", sfera, rt, 20);

    CloseWindow();
    return ProveEsito();
}
