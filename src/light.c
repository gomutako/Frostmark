#include "light.h"
#include "rlgl.h"
#include "raymath.h"
#include "config.h"
#include <stddef.h>
#include <math.h>

/* DUE mappe, non una. Con una sola bisogna scegliere fra ombre nitide e ombre
 * lontane: a 2048 texel su 80 m un texel copre 3,9 cm, che a tre metri dalla
 * camera sono quasi sette pixel di schermo - i blocchi si vedono. La mappa
 * vicina spende gli stessi texel su un quadrato piccolo, dove l'occhio guarda;
 * quella lontana copre il resto, dove un texel grosso non si distingue.
 *
 * Il quadrato conta: la mappa e' quadrata, quindi la proiezione deve esserlo -
 * vedi LightShadowBegin. */
#define SHADOW_RES        2048
#define SHADOW_NEAR_R     24.0f      /* 2,3 cm per texel */
#define SHADOW_FAR_R      60.0f      /* 5,9 cm per texel */
#define SHADOW_SPLIT      20.0f      /* oltre questa distanza si usa la lontana */
#define SHADOW_DEPTH      180.0f
#define SHADOW_CASCADES   2

static Shader   gShader;
static bool     gReady;
static RenderTexture2D gMap[SHADOW_CASCADES];

static int locLightDir, locSunAmount, locDepthOnly, locShadowOn, locShadowRes;
static int locLightVP[SHADOW_CASCADES], locShadowMap[SHADOW_CASCADES];
static int locViewPos, locSplit;

static Vector3 gSunDir  = { 0.0f, 1.0f, 0.0f };
static float   gSunAmt  = 1.0f;
static Matrix  gLightVP[SHADOW_CASCADES];
static int     gPass;                    /* cascata in corso di disegno */

/* Framebuffer di sola profondita': il colore non serve, e non allocarlo fa
 * risparmiare memoria e banda. Vedi l'esempio shaders_shadowmap di raylib. */
static RenderTexture2D LoadDepthFbo(int w, int h)
{
    RenderTexture2D t = { 0 };
    t.id = rlLoadFramebuffer();
    t.texture.width = w;
    t.texture.height = h;
    if (t.id == 0) return t;

    rlEnableFramebuffer(t.id);
    t.depth.id      = rlLoadTextureDepth(w, h, false);
    t.depth.width   = w;
    t.depth.height  = h;
    t.depth.format  = 19;              /* DEPTH_COMPONENT_24BIT */
    t.depth.mipmaps = 1;
    rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(t.id)) TraceLog(LOG_WARNING, "LUCE: framebuffer incompleto");
    rlDisableFramebuffer();
    return t;
}

bool LightInit(void)
{
    if (!FileExists("assets/shaders/scene.vs") || !FileExists("assets/shaders/scene.fs")) {
        TraceLog(LOG_INFO, "LUCE: assets/shaders/ assente, scena senza luce");
        return false;
    }

    gShader = LoadShader("assets/shaders/scene.vs", "assets/shaders/scene.fs");
    if (gShader.id == 0) { TraceLog(LOG_WARNING, "LUCE: shader non compilato"); return false; }

    locLightDir  = GetShaderLocation(gShader, "lightDir");
    locSunAmount = GetShaderLocation(gShader, "sunAmount");
    locDepthOnly = GetShaderLocation(gShader, "depthOnly");
    locShadowOn  = GetShaderLocation(gShader, "shadowOn");
    locShadowRes = GetShaderLocation(gShader, "shadowRes");
    locLightVP[0]   = GetShaderLocation(gShader, "lightVP0");
    locLightVP[1]   = GetShaderLocation(gShader, "lightVP1");
    locShadowMap[0] = GetShaderLocation(gShader, "shadowMap0");
    locShadowMap[1] = GetShaderLocation(gShader, "shadowMap1");
    locViewPos      = GetShaderLocation(gShader, "viewPos");
    locSplit        = GetShaderLocation(gShader, "splitDist");

    for (int i = 0; i < SHADOW_CASCADES; i++) gMap[i] = LoadDepthFbo(SHADOW_RES, SHADOW_RES);
    gReady = true;

    int res = SHADOW_RES;
    SetShaderValue(gShader, locShadowRes, &res, SHADER_UNIFORM_INT);
    TraceLog(LOG_INFO, "LUCE: sole e ombre attive, due mappe %dx%d: "
                       "vicina %.0f m (%.1f cm/texel), lontana %.0f m (%.1f cm/texel)",
             SHADOW_RES, SHADOW_RES, SHADOW_NEAR_R * 2.0f,
             SHADOW_NEAR_R * 200.0f / SHADOW_RES, SHADOW_FAR_R * 2.0f,
             SHADOW_FAR_R * 200.0f / SHADOW_RES);
    return true;
}

void LightUnload(void)
{
    if (!gReady) return;
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        if (gMap[i].depth.id > 0) rlUnloadTexture(gMap[i].depth.id);
        if (gMap[i].id > 0)       rlUnloadFramebuffer(gMap[i].id);
    }
    UnloadShader(gShader);
    gReady = false;
}

bool  LightReady(void)              { return gReady; }
int   LightCascades(void)           { return SHADOW_CASCADES; }
float LightShadowRadius(int cascade) { return cascade == 0 ? SHADOW_NEAR_R : SHADOW_FAR_R; }

/* --- La normale piatta ---------------------------------------------------
 * raylib lega 'texture2' allo shader SOLO se il materiale ha davvero una
 * normal map. Senza, l'uniform resterebbe a zero, cioe' allo stesso slot
 * dell'albedo, e lo shader leggerebbe il colore come se fosse un rilievo:
 * tutti gli asset che una normal map non ce l'hanno - cioe' tutti quelli di
 * oggi - si illuminerebbero a caso.
 *
 * Rimedio: chi non ne ha una ne riceve una piatta, un pixel (128,128,255),
 * che in spazio tangente vale (0,0,1) e vuol dire "non piegare niente". Cosi'
 * lo shader non ha bisogno di sapere come stanno le cose e non ha rami.
 *
 * Una copia per materiale, non una condivisa: UnloadMaterial() e UnloadModel()
 * liberano le texture delle mappe, e una texture sola liberata due volte da'
 * un guaio che si manifesta lontano da dove e' stato commesso. Un pixel per
 * materiale non si misura. */
/* --- Le tangenti ----------------------------------------------------------
 * Dicono come sta ruotata la texture sulla superficie: senza, una normal map
 * vera illumina storto, perche' lo shader non sa da che parte guarda la "u"
 * della mappa. raylib le legge dal .glb quando il pacchetto le ha esportate;
 * quando mancano vanno calcolate dalle UV.
 *
 * Perche' non GenMeshTangents() di raylib: quella legge i vertici a gruppi di
 * tre e ignora mesh->indices. Le mesh glTF sono quasi sempre indicizzate - i
 * vertici sono condivisi fra i triangoli - e su quelle costruirebbe triangoli
 * che non esistono. Su una mesh con vertexCount non multiplo di tre lascia
 * perfino valori non inizializzati, e lo dice nel log.
 *
 * Il conto e' quello classico: per ogni triangolo si ricava la direzione in
 * cui cresce la u, la si accumula sui suoi tre vertici - cosi' i vertici
 * condivisi mediano, e la superficie non si spezza sui bordi - e alla fine si
 * raddrizza rispetto alla normale. La w e' il verso della bitangente: i due
 * versi esistono entrambi e sbagliarlo ribalta il rilievo.
 *
 * Dove le UV sono degeneri (triangolo con area nulla nella texture) la
 * tangente resta nulla: lo shader se ne accorge e torna alla normale del
 * vertice, che e' esattamente il comportamento di prima. */
static void BuildTangents(Mesh *m)
{
    if (m->vertices == NULL || m->texcoords == NULL || m->normals == NULL) return;
    if (m->vertexCount <= 0 || m->triangleCount <= 0) return;

    int n = m->vertexCount;
    Vector3 *du = (Vector3 *)MemAlloc((unsigned int)(n * sizeof(Vector3)));
    Vector3 *dv = (Vector3 *)MemAlloc((unsigned int)(n * sizeof(Vector3)));
    float   *out = (float *)MemAlloc((unsigned int)(n * 4 * sizeof(float)));
    if (du == NULL || dv == NULL || out == NULL) {
        MemFree(du); MemFree(dv); MemFree(out);
        return;
    }

    for (int t = 0; t < m->triangleCount; t++) {
        int i[3];
        if (m->indices != NULL) {
            i[0] = m->indices[t * 3 + 0];
            i[1] = m->indices[t * 3 + 1];
            i[2] = m->indices[t * 3 + 2];
        } else {
            i[0] = t * 3 + 0; i[1] = t * 3 + 1; i[2] = t * 3 + 2;
        }
        if (i[0] < 0 || i[1] < 0 || i[2] < 0) continue;
        if (i[0] >= n || i[1] >= n || i[2] >= n) continue;

        const float *p0 = &m->vertices[i[0] * 3], *p1 = &m->vertices[i[1] * 3],
                    *p2 = &m->vertices[i[2] * 3];
        const float *w0 = &m->texcoords[i[0] * 2], *w1 = &m->texcoords[i[1] * 2],
                    *w2 = &m->texcoords[i[2] * 2];

        float e1x = p1[0] - p0[0], e1y = p1[1] - p0[1], e1z = p1[2] - p0[2];
        float e2x = p2[0] - p0[0], e2y = p2[1] - p0[1], e2z = p2[2] - p0[2];
        float s1 = w1[0] - w0[0], t1 = w1[1] - w0[1];
        float s2 = w2[0] - w0[0], t2 = w2[1] - w0[1];

        float det = s1 * t2 - s2 * t1;
        if (fabsf(det) < 1e-12f) continue;      /* UV degeneri: si salta */
        float r = 1.0f / det;

        Vector3 sd = { (t2 * e1x - t1 * e2x) * r,
                       (t2 * e1y - t1 * e2y) * r,
                       (t2 * e1z - t1 * e2z) * r };
        Vector3 td = { (s1 * e2x - s2 * e1x) * r,
                       (s1 * e2y - s2 * e1y) * r,
                       (s1 * e2z - s2 * e1z) * r };

        for (int k = 0; k < 3; k++) {
            du[i[k]] = Vector3Add(du[i[k]], sd);
            dv[i[k]] = Vector3Add(dv[i[k]], td);
        }
    }

    for (int v = 0; v < n; v++) {
        Vector3 nrm = { m->normals[v * 3 + 0], m->normals[v * 3 + 1],
                        m->normals[v * 3 + 2] };
        Vector3 tan = du[v];

        /* Gram-Schmidt: la parte di tangente che sta nel piano della faccia. */
        Vector3 tg = Vector3Subtract(tan, Vector3Scale(nrm, Vector3DotProduct(nrm, tan)));
        if (Vector3LengthSqr(tg) < 1e-12f) continue;   /* resta (0,0,0,0) */
        tg = Vector3Normalize(tg);

        out[v * 4 + 0] = tg.x;
        out[v * 4 + 1] = tg.y;
        out[v * 4 + 2] = tg.z;
        out[v * 4 + 3] =
            (Vector3DotProduct(Vector3CrossProduct(nrm, tg), dv[v]) < 0.0f) ? -1.0f : 1.0f;
    }

    MemFree(du);
    MemFree(dv);
    m->tangents = out;

    /* Portarle sulla scheda: la mesh e' gia' caricata, quindi o si aggiorna il
     * buffer che c'e' o se ne crea uno e lo si aggancia al vertex array. */
    if (m->vboId != NULL) {
        int slot = RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT;
        unsigned int bytes = (unsigned int)(n * 4 * sizeof(float));
        if (m->vboId[slot] != 0) rlUpdateVertexBuffer(m->vboId[slot], m->tangents, (int)bytes, 0);
        else m->vboId[slot] = rlLoadVertexBuffer(m->tangents, (int)bytes, false);

        rlEnableVertexArray(m->vaoId);
        rlSetVertexAttribute(slot, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(slot);
        rlDisableVertexArray();
    }
}

static void FitFlatNormal(Material *m)
{
    if (m->maps[MATERIAL_MAP_NORMAL].texture.id != 0) return;

    Image im = GenImageColor(1, 1, (Color){ 128, 128, 255, 255 });
    m->maps[MATERIAL_MAP_NORMAL].texture = LoadTextureFromImage(im);
    m->maps[MATERIAL_MAP_NORMAL].color   = WHITE;
    UnloadImage(im);
}

void LightApplyToMaterial(Material *m)
{
    if (!gReady || m == NULL) return;
    m->shader = gShader;
    FitFlatNormal(m);
}

void LightApplyToModel(Model *m)
{
    if (!gReady || m == NULL) return;

    /* Le tangenti prima della normale piatta: dopo, ogni materiale avrebbe una
     * normal map e non si distinguerebbe piu' quella vera dal tappabuchi.
     *
     * Le tangenti dicono come sta ruotata la texture sulla superficie, e senza
     * di esse una normal map vera illumina storto. raylib le legge dal .glb se
     * il pacchetto le ha esportate; se mancano le calcola BuildTangents(), una
     * volta al caricamento e non a ogni fotogramma. */
    for (int i = 0; i < m->meshCount; i++) {
        if (m->meshes[i].tangents != NULL) continue;
        int mat = (m->meshMaterial != NULL) ? m->meshMaterial[i] : 0;
        if (mat < 0 || mat >= m->materialCount) continue;
        if (m->materials[mat].maps[MATERIAL_MAP_NORMAL].texture.id == 0) continue;
        BuildTangents(&m->meshes[i]);
    }

    for (int i = 0; i < m->materialCount; i++) LightApplyToMaterial(&m->materials[i]);
}

void LightSetSun(Vector3 dirToSun, float amount)
{
    gSunDir = Vector3Normalize(dirToSun);
    gSunAmt = amount;
}

/* Proiezione ortogonale: il sole e' lontano, i suoi raggi sono paralleli. Il
 * volume segue il giocatore, quindi la mappa e' sempre spesa dove si guarda. */
/* Il quadrato non sta centrato sul giocatore ma spostato VERSO il sole. Le
 * ombre cadono dalla parte opposta al sole, quindi cio' che puo' oscurare il
 * giocatore sta dalla parte del sole: centrando sul giocatore, una torre a
 * venti metri restava fuori dalla mappa e la sua ombra spariva - e' successo
 * davvero, alla prima prova con due mappe. */
Vector3 LightShadowCenter(Vector3 base, int cascade)
{
    Vector3 s = { gSunDir.x, 0.0f, gSunDir.z };
    float len = sqrtf(s.x * s.x + s.z * s.z);
    if (len > 0.001f) { s.x /= len; s.z /= len; }
    float shift = LightShadowRadius(cascade) * 0.55f;
    return (Vector3){ base.x + s.x * shift, base.y, base.z + s.z * shift };
}

void LightShadowBegin(Vector3 base, int cascade)
{
    if (!gReady) return;
    gPass = cascade;

    float radius = LightShadowRadius(cascade);
    Vector3 center = LightShadowCenter(base, cascade);

    Camera3D lightCam = { 0 };
    lightCam.position   = Vector3Add(center, Vector3Scale(gSunDir, SHADOW_DEPTH * 0.5f));
    lightCam.target     = center;
    lightCam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    /* con il sole allo zenit 'up' e la direzione sarebbero paralleli */
    if (fabsf(gSunDir.y) > 0.99f) lightCam.up = (Vector3){ 0.0f, 0.0f, 1.0f };
    lightCam.fovy       = radius * 2.0f;
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    /* BeginTextureMode e non rlEnableFramebuffer a mano: e' l'unico modo per
     * cui BeginMode3D sappia che sta disegnando in un quadrato. Con il
     * framebuffer acceso a mano prendeva l'aspetto dello SCHERMO, e la
     * proiezione copriva 124 m in orizzontale contro 70 in verticale, tutti
     * schiacciati negli stessi 1024 texel: le ombre uscivano a scaletta. */
    BeginTextureMode(gMap[cascade]);
    rlClearScreenBuffers();

    BeginMode3D(lightCam);
    gLightVP[cascade] = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    int one = 1;
    SetShaderValue(gShader, locDepthOnly, &one, SHADER_UNIFORM_INT);
}

void LightShadowEnd(void)
{
    if (!gReady) return;

    EndMode3D();
    EndTextureMode();

    int zero = 0;
    SetShaderValue(gShader, locDepthOnly, &zero, SHADER_UNIFORM_INT);
}

void LightFrame(Camera3D cam)
{
    if (!gReady) return;
    (void)cam;

    SetShaderValue(gShader, locLightDir,  &gSunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, locSunAmount, &gSunAmt, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gShader, locViewPos,   &cam.position, SHADER_UNIFORM_VEC3);
    float split = SHADOW_SPLIT;
    SetShaderValue(gShader, locSplit, &split, SHADER_UNIFORM_FLOAT);

    for (int i = 0; i < SHADOW_CASCADES; i++)
        SetShaderValueMatrix(gShader, locLightVP[i], gLightVP[i]);

    int on = (gMap[0].depth.id > 0) ? 1 : 0;
    SetShaderValue(gShader, locShadowOn, &on, SHADER_UNIFORM_INT);

    /* Le mappe vivono in slot alti: i bassi servono alle texture del
     * materiale, e raylib li riassegna a ogni DrawMesh. */
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        rlActiveTextureSlot(10 + i);
        rlEnableTexture(gMap[i].depth.id);
        int slot = 10 + i;
        SetShaderValue(gShader, locShadowMap[i], &slot, SHADER_UNIFORM_INT);
    }
}
