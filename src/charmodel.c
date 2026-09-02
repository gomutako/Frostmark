#include "charmodel.h"
#include "light.h"
#include "raymath.h"
#include <string.h>

/* Nomi cercati per ogni ruolo, in ordine di preferenza: prima uguaglianza
 * esatta, poi sottostringa, sempre ignorando maiuscole e minuscole. Cosi'
 * pacchetti diversi funzionano senza toccare il codice - i personaggi KayKit
 * ("Walking_A"), il robot di Quaternius ("Robot_Walking"), il greenman degli
 * esempi di raylib ("2_move"). */
static const char *ANIM_WANTED[CANIM_COUNT][4] = {
    { "Idle",    "Robot_Idle",    "1_idle",   "idle"   },   /* CANIM_IDLE   */
    { "Walking_A", "Robot_Walking", "2_move", "walk"   },   /* CANIM_WALK   */
    { "Running_A", "Robot_Running", "run",    NULL     },   /* CANIM_RUN    */
    { "1H_Melee_Attack_Slice_Diagonal", "Robot_Punch",
      "3_attack", "attack" },                               /* CANIM_ATTACK */
    { "Blocking", "Block",        "block",    NULL     },   /* CANIM_BLOCK  */
    { "Spellcast_Shoot", "Spellcasting", "cast", NULL  },   /* CANIM_CAST   */
    { "Jump_Idle", "Robot_Jump",  "jump",     NULL     },   /* CANIM_JUMP   */
    { "Hit_A",   "hit",           NULL,       NULL     },   /* CANIM_HURT   */
    { "Death_A", "Robot_Death",   "death",    NULL     },   /* CANIM_DEATH  */
};

static const char *ROLE_NAME[CANIM_COUNT] = {
    "riposo", "camminata", "corsa", "attacco", "parata",
    "incantesimo", "salto", "colpito", "morte"
};

/* ------------------------------------------------------------------------ */
/*  CONFRONTI SENZA DISTINZIONE DI MAIUSCOLE                                */
/* ------------------------------------------------------------------------ */

static char LowerChar(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool SameNoCase(const char *a, const char *b)
{
    while (*a && *b) { if (LowerChar(*a) != LowerChar(*b)) return false; a++; b++; }
    return *a == *b;
}

static bool ContainsNoCase(const char *hay, const char *needle)
{
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && LowerChar(*h) == LowerChar(*n)) { h++; n++; }
        if (*n == '\0') return true;
    }
    return false;
}

static int FindAnim(const ModelAnimation *anims, int count, const char *wanted[4])
{
    for (int w = 0; w < 4 && wanted[w]; w++)
        for (int i = 0; i < count; i++)
            if (SameNoCase(anims[i].name, wanted[w])) return i;

    for (int w = 0; w < 4 && wanted[w]; w++)
        for (int i = 0; i < count; i++)
            if (ContainsNoCase(anims[i].name, wanted[w])) return i;

    return -1;
}

static int FindBone(const Model *m, const char *name)
{
    for (int i = 0; i < m->boneCount; i++)
        if (SameNoCase(m->bones[i].name, name)) return i;
    return -1;
}

/* ------------------------------------------------------------------------ */
/*  CARICAMENTO                                                             */
/* ------------------------------------------------------------------------ */

/* raylib 5.5 non regge i modelli misti: UpdateModelAnimation() legge
 * mesh.boneWeights su TUTTE le mesh del modello, comprese quelle senza pesi -
 * armi, scudi ed elmi che nei pacchetti di personaggi sono mesh separate
 * agganciate a un osso - e dereferenzia un puntatore nullo, cioe' crolla.
 *
 * Si costruisce quindi una vista del modello con le sole mesh skinnate. Le
 * strutture Mesh sono copie che puntano agli stessi dati e agli stessi buffer
 * GPU, percio' deformare la vista aggiorna cio' che poi si disegna.
 * Da liberare: solo i due array, non le mesh (le possiede 'model'). */
static void BuildSkinnedView(CharModel *c)
{
    c->skinnedView   = c->model;
    c->viewAllocated = false;
    c->anySkinned    = false;

    int n = 0;
    for (int i = 0; i < c->model.meshCount; i++)
        if (c->model.meshes[i].boneIds != NULL) { n++; c->anySkinned = true; }
    if (n == 0 || n == c->model.meshCount) return;      /* niente da filtrare */

    Mesh *meshes = (Mesh *)MemAlloc((unsigned int)n * sizeof(Mesh));
    int  *mats   = (int  *)MemAlloc((unsigned int)n * sizeof(int));
    int k = 0;
    for (int i = 0; i < c->model.meshCount; i++) {
        if (c->model.meshes[i].boneIds == NULL) continue;
        meshes[k] = c->model.meshes[i];
        mats[k]   = c->model.meshMaterial[i];
        k++;
    }
    c->skinnedView.meshCount    = n;
    c->skinnedView.meshes       = meshes;
    c->skinnedView.meshMaterial = mats;
    c->viewAllocated = true;
}

/* Legge il file "<modello>.attach": righe "<indice mesh> <nome osso>", con '#'
 * per i commenti. Lo genera tools/glb_attachments.py, che conosce i nomi
 * originali delle mesh - informazione che raylib non conserva. */
static void LoadAttachments(CharModel *c, const char *glbPath)
{
    c->attachCount = 0;

    char path[512];
    int n = (int)strlen(glbPath);
    if (n < 4 || n >= (int)sizeof(path) - 8) return;
    memcpy(path, glbPath, (size_t)n - 4);              /* toglie ".glb" */
    memcpy(path + n - 4, ".attach", 8);

    if (!FileExists(path)) return;
    char *text = LoadFileText(path);
    if (text == NULL) return;

    const char *s = text;
    while (*s && c->attachCount < MAX_ATTACHMENTS) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        if (*s == '\0') break;
        if (*s == '#') { while (*s && *s != '\n') s++; continue; }

        int mesh = 0;
        bool digits = false;
        while (*s >= '0' && *s <= '9') { mesh = mesh * 10 + (*s - '0'); s++; digits = true; }
        while (*s == ' ' || *s == '\t') s++;

        char bone[64];
        int b = 0;
        while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n' &&
               *s != '#' && b < (int)sizeof(bone) - 1) bone[b++] = *s++;
        bone[b] = '\0';
        while (*s && *s != '\n') s++;

        if (!digits || b == 0) continue;

        int boneId = FindBone(&c->model, bone);
        if (mesh < 0 || mesh >= c->model.meshCount) {
            TraceLog(LOG_WARNING, "CHAR: %s: mesh %d fuori dal modello", path, mesh);
        } else if (boneId < 0) {
            TraceLog(LOG_WARNING, "CHAR: %s: osso '%s' inesistente", path, bone);
        } else if (c->model.meshes[mesh].boneIds != NULL) {
            TraceLog(LOG_WARNING, "CHAR: %s: la mesh %d ha gia' i pesi", path, mesh);
        } else {
            c->attach[c->attachCount].mesh = mesh;
            c->attach[c->attachCount].bone = boneId;
            c->attachCount++;
        }
    }
    UnloadFileText(text);
}

/* Altezza del modello nella posa di riposo, contando solo le mesh animate:
 * armi ed elmi sporgono e falserebbero la misura. */
static float RestHeight(const CharModel *c)
{
    float lo = 1e30f, hi = -1e30f;
    for (int i = 0; i < c->model.meshCount; i++) {
        if (c->anySkinned && c->model.meshes[i].boneIds == NULL) continue;
        BoundingBox b = GetMeshBoundingBox(c->model.meshes[i]);
        if (b.min.y < lo) lo = b.min.y;
        if (b.max.y > hi) hi = b.max.y;
    }
    return (hi > lo) ? (hi - lo) : 0.0f;
}

/* Un pacchetto di personaggi porta 76-95 clip, al gioco ne servono nove: le
 * altre occupano memoria per niente, ed e' la voce dominante (una posa e' 41
 * ossa x 40 byte, per ogni fotogramma di ogni clip). Le pose inutilizzate si
 * liberano subito; azzerando frameCount, UnloadModelAnimations() non le
 * ripassa - libera comunque l'array delle righe e le ossa. */
static void DropUnusedAnims(CharModel *c)
{
    int freedFrames = 0;
    for (int i = 0; i < c->animCount; i++) {
        bool used = false;
        for (int r = 0; r < CANIM_COUNT; r++) if (c->animOf[r] == i) used = true;
        if (used) continue;

        for (int f = 0; f < c->anims[i].frameCount; f++) {
            MemFree(c->anims[i].framePoses[f]);
            c->anims[i].framePoses[f] = NULL;
            freedFrames++;
        }
        c->anims[i].frameCount = 0;
    }
    if (freedFrames > 0)
        TraceLog(LOG_INFO, "CHAR:   liberate le pose di %d fotogrammi non usati",
                 freedFrames);
}

bool CharModelLoad(CharModel *c, const char *glbPath, float heightMeters)
{
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < CANIM_COUNT; i++) c->animOf[i] = -1;
    c->scale = 1.0f;

    if (!FileExists(glbPath)) return false;

    Model m = LoadModel(glbPath);
    if (m.meshCount == 0) {                  /* formato non supportato o rotto */
        TraceLog(LOG_WARNING, "CHAR: %s non caricato", glbPath);
        UnloadModel(m);
        return false;
    }
    LightApplyToModel(&m);
    c->model  = m;
    c->loaded = true;

    BuildSkinnedView(c);
    LoadAttachments(c, glbPath);

    float h = RestHeight(c);
    if (h > 0.01f && heightMeters > 0.01f) c->scale = heightMeters / h;

    c->anims = LoadModelAnimations(glbPath, &c->animCount);
    for (int r = 0; r < CANIM_COUNT; r++)
        c->animOf[r] = FindAnim(c->anims, c->animCount, ANIM_WANTED[r]);
    DropUnusedAnims(c);

    TraceLog(LOG_INFO, "CHAR: %s (%d mesh, %d ossa, %d animazioni, %d agganci, "
                       "alto %.2f -> scala %.2f)",
             glbPath, m.meshCount, m.boneCount, c->animCount, c->attachCount,
             h, c->scale);
    for (int r = 0; r < CANIM_COUNT; r++)
        if (c->animOf[r] >= 0)
            TraceLog(LOG_INFO, "CHAR:   %-12s -> %s", ROLE_NAME[r],
                     c->anims[c->animOf[r]].name);
    return true;
}

void CharModelUnload(CharModel *c)
{
    if (!c->loaded) return;
    if (c->viewAllocated) {
        MemFree(c->skinnedView.meshes);          /* solo gli array della vista */
        MemFree(c->skinnedView.meshMaterial);
        c->viewAllocated = false;
    }
    if (c->anims) UnloadModelAnimations(c->anims, c->animCount);
    UnloadModel(c->model);
    c->anims = NULL;
    c->animCount = 0;
    c->loaded = false;
}

/* ------------------------------------------------------------------------ */
/*  POSE                                                                    */
/* ------------------------------------------------------------------------ */

int CharModelFrames(const CharModel *c, CharAnim a)
{
    if (!c->loaded || c->animOf[a] < 0) return 0;
    return c->anims[c->animOf[a]].frameCount;
}

bool CharAnimLoops(CharAnim a)
{
    return a == CANIM_IDLE || a == CANIM_WALK || a == CANIM_RUN ||
           a == CANIM_BLOCK || a == CANIM_JUMP;
}

CharAnim CharModelResolve(const CharModel *c, CharAnim want)
{
    if (CharModelFrames(c, want) > 0) return want;
    if (want == CANIM_RUN   && CharModelFrames(c, CANIM_WALK)  > 0) return CANIM_WALK;
    if (want == CANIM_BLOCK && CharModelFrames(c, CANIM_IDLE)  > 0) return CANIM_IDLE;
    return CANIM_IDLE;
}

float CharModelAdvance(const CharModel *c, CharAnim a, float frame, float dt,
                       float speedMul, float oneShotSeconds)
{
    int frames = CharModelFrames(c, a);
    if (frames <= 0) return 0.0f;

    float fps;
    if (CharAnimLoops(a)) {
        fps = 60.0f * speedMul;
    } else {
        if (oneShotSeconds < 0.05f) oneShotSeconds = 0.05f;
        fps = (float)frames / oneShotSeconds;
    }
    frame += dt * fps;

    if (CharAnimLoops(a)) {
        while (frame >= (float)frames) frame -= (float)frames;
        if (frame < 0.0f) frame = 0.0f;
    } else if (frame > (float)(frames - 1)) {
        frame = (float)(frames - 1);
    }
    return frame;
}

/* Trasformazione che porta una mesh dalla posa di riposo a quella corrente
 * seguendo un osso. E' la stessa composizione che raylib usa per le ossa in
 * UpdateModelAnimationBones(): replicarla garantisce che l'arma e il corpo si
 * muovano insieme. I vertici delle mesh agganciate sono gia' in spazio modello
 * (raylib ci cuoce dentro la trasformazione del nodo al caricamento), percio'
 * qui serve solo posaCorrente x inversa(posaDiRiposo). */
static Matrix BoneMatrix(const CharModel *c, int bone,
                         const ModelAnimation *anim, int frame)
{
    Transform in  = c->model.bindPose[bone];
    Transform out = anim->framePoses[frame][bone];

    Vector3    invT = Vector3RotateByQuaternion(Vector3Negate(in.translation),
                                                QuaternionInvert(in.rotation));
    Quaternion invR = QuaternionInvert(in.rotation);
    Vector3    invS = Vector3Divide((Vector3){ 1.0f, 1.0f, 1.0f }, in.scale);

    Vector3 t = Vector3Add(Vector3RotateByQuaternion(
                               Vector3Multiply(out.scale, invT), out.rotation),
                           out.translation);
    Quaternion r = QuaternionMultiply(out.rotation, invR);
    Vector3 sc = Vector3Multiply(out.scale, invS);

    return MatrixMultiply(MatrixMultiply(QuaternionToMatrix(r),
                                         MatrixTranslate(t.x, t.y, t.z)),
                          MatrixScale(sc.x, sc.y, sc.z));
}

static void DrawOne(const CharModel *c, int mesh, Matrix xform, Color tint)
{
    Material mat = c->model.materials[c->model.meshMaterial[mesh]];
    Color keep = mat.maps[MATERIAL_MAP_DIFFUSE].color;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;   /* ciclo giorno/notte */
    DrawMesh(c->model.meshes[mesh], mat, xform);
    mat.maps[MATERIAL_MAP_DIFFUSE].color = keep;
}

void CharModelDraw(const CharModel *c, Vector3 pos, float yawRad,
                   CharAnim a, float frame, Color tint)
{
    if (!c->loaded) return;

    const ModelAnimation *anim = NULL;
    int f = 0;
    if (c->animOf[a] >= 0 && c->anims[c->animOf[a]].frameCount > 0) {
        anim = &c->anims[c->animOf[a]];
        f = (int)frame % anim->frameCount;
        if (f < 0) f = 0;
        /* La deformazione vale fino alla prossima chiamata: e' per questo che
         * un solo modello serve molti personaggi con pose diverse. */
        UpdateModelAnimation(c->skinnedView, *anim, f);
    }

    float s = c->scale;
    Matrix xform = MatrixMultiply(
        MatrixMultiply(MatrixScale(s, s, s), MatrixRotateY(yawRad)),
        MatrixTranslate(pos.x, pos.y, pos.z));

    for (int i = 0; i < c->model.meshCount; i++) {
        if (c->anySkinned && c->model.meshes[i].boneIds == NULL) continue;
        DrawOne(c, i, xform, tint);
    }

    /* Arma, scudo, elmo: mesh senza pesi, spostate a mano con l'osso. */
    for (int k = 0; k < c->attachCount; k++) {
        Matrix m = anim ? MatrixMultiply(BoneMatrix(c, c->attach[k].bone, anim, f), xform)
                        : xform;
        DrawOne(c, c->attach[k].mesh, m, tint);
    }
}
