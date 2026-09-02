#include "light.h"
#include "rlgl.h"
#include "raymath.h"
#include "config.h"
#include <stddef.h>

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

void LightApplyToMaterial(Material *m)
{
    if (gReady && m != NULL) m->shader = gShader;
}

void LightApplyToModel(Model *m)
{
    if (!gReady || m == NULL) return;
    for (int i = 0; i < m->materialCount; i++) m->materials[i].shader = gShader;
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
