#include "light.h"
#include "rlgl.h"
#include "raymath.h"
#include "config.h"
#include <stddef.h>

/* La mappa e' quadrata e segue il giocatore. Piu' e' larga, piu' ombre si
 * vedono ma piu' grossolane: a 1024 texel su 70 metri un texel copre 7 cm,
 * che su un tronco si vede appena. */
#define SHADOW_RES     1024
#define SHADOW_RADIUS  35.0f
#define SHADOW_DEPTH   180.0f

static Shader   gShader;
static bool     gReady;
static RenderTexture2D gMap;

static int locLightDir, locSunAmount, locDepthOnly, locShadowOn, locShadowRes;
static int locLightVP, locShadowMap, locViewPos;

static Vector3 gSunDir  = { 0.0f, 1.0f, 0.0f };
static float   gSunAmt  = 1.0f;
static Matrix  gLightVP;

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
    locLightVP   = GetShaderLocation(gShader, "lightVP");
    locShadowMap = GetShaderLocation(gShader, "shadowMap");
    locViewPos   = GetShaderLocation(gShader, "viewPos");
    (void)locViewPos;

    gMap = LoadDepthFbo(SHADOW_RES, SHADOW_RES);
    gReady = true;

    int res = SHADOW_RES;
    SetShaderValue(gShader, locShadowRes, &res, SHADER_UNIFORM_INT);
    TraceLog(LOG_INFO, "LUCE: sole e ombre attivi (mappa %dx%d su %.0f m)",
             SHADOW_RES, SHADOW_RES, SHADOW_RADIUS * 2.0f);
    return true;
}

void LightUnload(void)
{
    if (!gReady) return;
    if (gMap.depth.id > 0) rlUnloadTexture(gMap.depth.id);
    if (gMap.id > 0)       rlUnloadFramebuffer(gMap.id);
    UnloadShader(gShader);
    gReady = false;
}

bool  LightReady(void)         { return gReady; }
float LightShadowRadius(void)  { return SHADOW_RADIUS; }

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
void LightShadowBegin(Vector3 center)
{
    if (!gReady) return;

    Camera3D lightCam = { 0 };
    lightCam.position   = Vector3Add(center, Vector3Scale(gSunDir, SHADOW_DEPTH * 0.5f));
    lightCam.target     = center;
    lightCam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    /* con il sole allo zenit 'up' e la direzione sarebbero paralleli */
    if (fabsf(gSunDir.y) > 0.99f) lightCam.up = (Vector3){ 0.0f, 0.0f, 1.0f };
    lightCam.fovy       = SHADOW_RADIUS * 2.0f;
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    rlEnableFramebuffer(gMap.id);
    rlViewport(0, 0, SHADOW_RES, SHADOW_RES);
    rlClearScreenBuffers();

    BeginMode3D(lightCam);
    gLightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    int one = 1;
    SetShaderValue(gShader, locDepthOnly, &one, SHADER_UNIFORM_INT);
}

void LightShadowEnd(void)
{
    if (!gReady) return;

    EndMode3D();
    rlDisableFramebuffer();
    rlViewport(0, 0, GetScreenWidth(), GetScreenHeight());

    int zero = 0;
    SetShaderValue(gShader, locDepthOnly, &zero, SHADER_UNIFORM_INT);
}

void LightFrame(Camera3D cam)
{
    if (!gReady) return;
    (void)cam;

    SetShaderValue(gShader, locLightDir,  &gSunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, locSunAmount, &gSunAmt, SHADER_UNIFORM_FLOAT);
    SetShaderValueMatrix(gShader, locLightVP, gLightVP);

    int on = (gMap.depth.id > 0) ? 1 : 0;
    SetShaderValue(gShader, locShadowOn, &on, SHADER_UNIFORM_INT);

    /* La mappa vive in uno slot alto: gli slot bassi servono alle texture del
     * materiale, e raylib li riassegna a ogni DrawMesh. */
    rlActiveTextureSlot(10);
    rlEnableTexture(gMap.depth.id);
    int slot = 10;
    SetShaderValue(gShader, locShadowMap, &slot, SHADER_UNIFORM_INT);
}
