#include "player.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define MOUSE_SENS 0.0026f

/* ------------------------------------------------------------------------ */
/*  MODELLO ANIMATO OPZIONALE                                               */
/* ------------------------------------------------------------------------ */

/* Nomi cercati per ogni ruolo, in ordine di preferenza: prima uguaglianza
 * esatta, poi sottostringa, sempre ignorando maiuscole e minuscole. Cosi'
 * pacchetti diversi funzionano senza toccare il codice - il cavaliere KayKit
 * ("Walking_A"), il robot di Quaternius ("Robot_Walking"), il greenman degli
 * esempi di raylib ("2_move"). */
static const char *ANIM_WANTED[PANIM_COUNT][4] = {
    { "Idle",    "Robot_Idle",    "1_idle",   "idle"   },   /* PANIM_IDLE   */
    { "Walking_A", "Robot_Walking", "2_move", "walk"   },   /* PANIM_WALK   */
    { "Running_A", "Robot_Running", "run",    NULL     },   /* PANIM_RUN    */
    { "1H_Melee_Attack_Slice_Diagonal", "Robot_Punch",
      "3_attack", "attack" },                               /* PANIM_ATTACK */
    { "Blocking", "Block",        "block",    NULL     },   /* PANIM_BLOCK  */
    { "Spellcast_Shoot", "Spellcasting", "cast", NULL  },   /* PANIM_CAST   */
    { "Jump_Idle", "Robot_Jump",  "jump",     NULL     },   /* PANIM_JUMP   */
    { "Hit_A",   "hit",           NULL,       NULL     },   /* PANIM_HURT   */
    { "Death_A", "Robot_Death",   "death",    NULL     },   /* PANIM_DEATH  */
};

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

/* Cerca l'animazione di un ruolo: prima i nomi esatti, poi le sottostringhe. */
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

/* raylib 5.5 non regge i modelli misti: UpdateModelAnimation() legge
 * mesh.boneWeights su TUTTE le mesh del modello, comprese quelle senza pesi -
 * armi, scudi ed elmi che nei pacchetti di personaggi sono mesh separate
 * agganciate a un osso - e dereferenzia un puntatore nullo, cioe' crolla.
 *
 * Si costruisce quindi una vista del modello con le sole mesh skinnate. Le
 * strutture Mesh sono copie che puntano agli stessi dati e agli stessi buffer
 * GPU, percio' deformare la vista aggiorna cio' che disegna PlayerDraw().
 * Da liberare: solo i due array, non le mesh (le possiede 'model'). */
static void BuildSkinnedView(Player *p)
{
    p->skinnedView   = p->model;
    p->viewAllocated = false;

    int n = 0;
    for (int i = 0; i < p->model.meshCount; i++)
        if (p->model.meshes[i].boneIds != NULL) n++;
    if (n == 0 || n == p->model.meshCount) return;      /* niente da filtrare */

    Mesh *meshes = (Mesh *)MemAlloc((unsigned int)n * sizeof(Mesh));
    int  *mats   = (int  *)MemAlloc((unsigned int)n * sizeof(int));
    int k = 0;
    for (int i = 0; i < p->model.meshCount; i++) {
        if (p->model.meshes[i].boneIds == NULL) continue;
        meshes[k] = p->model.meshes[i];
        mats[k]   = p->model.meshMaterial[i];
        k++;
    }
    p->skinnedView.meshCount    = n;
    p->skinnedView.meshes       = meshes;
    p->skinnedView.meshMaterial = mats;
    p->viewAllocated = true;

    TraceLog(LOG_INFO, "PLAYER: %d mesh su %d hanno lo scheletro; le altre "
                       "(armi, scudi, elmi) si muovono via %s",
             n, p->model.meshCount, PLAYER_ATTACH_FILE);
}

/* Indice dell'osso dato il nome, -1 se non c'e'. */
static int FindBone(const Model *m, const char *name)
{
    for (int i = 0; i < m->boneCount; i++)
        if (SameNoCase(m->bones[i].name, name)) return i;
    return -1;
}

/* Legge assets/models/player.attach: righe "<indice mesh> <nome osso>", con '#'
 * per i commenti. Il file lo genera tools/glb_attachments.py, che sa i nomi
 * originali delle mesh - informazione che raylib non conserva. */
static void LoadAttachments(Player *p)
{
    p->attachCount = 0;
    if (!FileExists(PLAYER_ATTACH_FILE)) return;

    char *text = LoadFileText(PLAYER_ATTACH_FILE);
    if (text == NULL) return;

    const char *c = text;
    while (*c && p->attachCount < MAX_ATTACHMENTS) {
        while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') c++;
        if (*c == '\0') break;
        if (*c == '#') {                              /* riga di commento */
            while (*c && *c != '\n') c++;
            continue;
        }

        int mesh = 0;
        bool digits = false;
        while (*c >= '0' && *c <= '9') { mesh = mesh * 10 + (*c - '0'); c++; digits = true; }
        while (*c == ' ' || *c == '\t') c++;

        char bone[64];
        int n = 0;
        while (*c && *c != ' ' && *c != '\t' && *c != '\r' && *c != '\n' &&
               *c != '#' && n < (int)sizeof(bone) - 1) bone[n++] = *c++;
        bone[n] = '\0';
        while (*c && *c != '\n') c++;                  /* resto della riga */

        if (!digits || n == 0) continue;

        int boneId = FindBone(&p->model, bone);
        if (mesh < 0 || mesh >= p->model.meshCount) {
            TraceLog(LOG_WARNING, "PLAYER: aggancio ignorato, mesh %d fuori dal modello", mesh);
        } else if (boneId < 0) {
            TraceLog(LOG_WARNING, "PLAYER: aggancio ignorato, osso '%s' inesistente", bone);
        } else if (p->model.meshes[mesh].boneIds != NULL) {
            TraceLog(LOG_WARNING, "PLAYER: aggancio ignorato, la mesh %d ha gia' i pesi", mesh);
        } else {
            p->attach[p->attachCount].mesh = mesh;
            p->attach[p->attachCount].bone = boneId;
            p->attachCount++;
            TraceLog(LOG_INFO, "PLAYER:   aggancio mesh %d -> osso %s", mesh, bone);
        }
    }
    UnloadFileText(text);
}

static void PlayerLoadModel(Player *p)
{
    for (int i = 0; i < PANIM_COUNT; i++) p->animOf[i] = -1;
    if (!FileExists(PLAYER_MODEL_FILE)) return;

    Model m = LoadModel(PLAYER_MODEL_FILE);
    if (m.meshCount == 0) {              /* formato non supportato o file rotto */
        TraceLog(LOG_WARNING, "PLAYER: %s non caricato", PLAYER_MODEL_FILE);
        UnloadModel(m);
        return;
    }
    p->model    = m;
    p->hasModel = true;

    /* raylib anima solo le mesh con pesi delle ossa. I pacchetti di personaggi
     * contengono spesso anche armi, scudi ed elmi come mesh separate agganciate
     * a un osso: quelle non vengono deformate, e le si sposta a mano seguendo
     * l'osso indicato in PLAYER_ATTACH_FILE (vedi LoadAttachments e
     * BoneMatrix). Il flag distingue i due gruppi in fase di disegno. */
    p->anySkinned = false;
    for (int i = 0; i < m.meshCount; i++)
        if (m.meshes[i].boneIds != NULL) p->anySkinned = true;

    BuildSkinnedView(p);
    LoadAttachments(p);

    p->anims = LoadModelAnimations(PLAYER_MODEL_FILE, &p->animCount);
    for (int r = 0; r < PANIM_COUNT; r++)
        p->animOf[r] = FindAnim(p->anims, p->animCount, ANIM_WANTED[r]);

    TraceLog(LOG_INFO, "PLAYER: modello %s (%d mesh, %d ossa, %d animazioni)",
             PLAYER_MODEL_FILE, m.meshCount, m.boneCount, p->animCount);
    static const char *ROLE_NAME[PANIM_COUNT] = {
        "riposo", "camminata", "corsa", "attacco", "parata",
        "incantesimo", "salto", "colpito", "morte"
    };
    for (int r = 0; r < PANIM_COUNT; r++) {
        if (p->animOf[r] >= 0)
            TraceLog(LOG_INFO, "PLAYER:   %-12s -> %s", ROLE_NAME[r],
                     p->anims[p->animOf[r]].name);
        else
            TraceLog(LOG_WARNING, "PLAYER:   %-12s -> assente nel pacchetto", ROLE_NAME[r]);
    }
}

void PlayerUnload(Player *p)
{
    if (!p->hasModel) return;
    if (p->viewAllocated) {
        MemFree(p->skinnedView.meshes);          /* solo gli array della vista */
        MemFree(p->skinnedView.meshMaterial);
        p->viewAllocated = false;
    }
    if (p->anims) UnloadModelAnimations(p->anims, p->animCount);
    UnloadModel(p->model);
    p->anims = NULL;
    p->animCount = 0;
    p->hasModel = false;
}

/* Quale ruolo mostrare, in ordine di priorita'. */
static PlayerAnim PickAnim(const Player *p)
{
    if (p->hp <= 0.0f)        return PANIM_DEATH;
    if (p->hurtFlash > 0.22f) return PANIM_HURT;
    if (p->swing > 0.05f)     return PANIM_ATTACK;
    if (p->castCd > 0.50f)    return PANIM_CAST;   /* castCd parte da 0.75 */
    if (p->blocking)          return PANIM_BLOCK;
    if (!p->onGround)         return PANIM_JUMP;
    if (p->moveSpeed > 0.2f)  return p->sprinting ? PANIM_RUN : PANIM_WALK;
    return PANIM_IDLE;
}

static bool AnimLoops(PlayerAnim a)
{
    return a == PANIM_IDLE || a == PANIM_WALK || a == PANIM_RUN ||
           a == PANIM_BLOCK || a == PANIM_JUMP;
}

/* Durata a schermo delle animazioni non ciclabili: quanto dura l'azione nel
 * gioco, non quanto e' lunga la clip. Il fendente del cavaliere KayKit dura 59
 * fotogrammi, cioe' circa un secondo, mentre il colpo si esaurisce in 0.4 s:
 * senza questo la spada si fermerebbe a meta' del movimento. */
static float OneShotSeconds(PlayerAnim a)
{
    switch (a) {
        case PANIM_ATTACK: return 0.42f;
        case PANIM_CAST:   return 0.30f;
        case PANIM_HURT:   return 0.28f;
        case PANIM_DEATH:  return 1.30f;
        default:           return 1.00f;
    }
}

void PlayerUpdateAnimation(Player *p, float dt)
{
    if (!p->hasModel || p->animCount <= 0) return;

    PlayerAnim want = PickAnim(p);
    if (p->animOf[want] < 0) {                     /* ruolo assente: si ripiega */
        if (want == PANIM_RUN)   want = PANIM_WALK;
        if (want == PANIM_BLOCK) want = PANIM_IDLE;
        if (p->animOf[want] < 0) want = PANIM_IDLE;
        if (p->animOf[want] < 0) return;
    }
    if (want != p->anim) { p->anim = want; p->animFrame = 0.0f; }

    int frames = p->anims[p->animOf[want]].frameCount;
    if (frames <= 0) return;

    /* Le animazioni glTF caricate da raylib sono ricampionate a 60 fps. */
    float fps = 60.0f;
    if (want == PANIM_WALK || want == PANIM_RUN) {
        /* il passo segue la velocita' reale, cosi' i piedi non slittano */
        float ref = (want == PANIM_RUN) ? PLAYER_RUN : PLAYER_WALK;
        fps *= p->moveSpeed / ref;
    } else if (!AnimLoops(want)) {
        fps = (float)frames / OneShotSeconds(want);
    }
    p->animFrame += dt * fps;

    if (AnimLoops(want)) {
        while (p->animFrame >= (float)frames) p->animFrame -= (float)frames;
    } else if (p->animFrame > (float)(frames - 1)) {
        p->animFrame = (float)(frames - 1);        /* resta sull'ultima posa */
    }

    /* La deformazione costa: in soggettiva il corpo non si vede. */
    if (p->camMode == CAM_THIRD)
        UpdateModelAnimation(p->skinnedView, p->anims[p->animOf[want]], (int)p->animFrame);
}

/* ------------------------------------------------------------------------ */

void PlayerInit(Player *p, Vector3 spawn)
{
    memset(p, 0, sizeof(Player));
    p->pos   = spawn;
    p->yaw   = 0.0f;
    p->pitch = -0.05f;

    p->camMode = CAM_FIRST;
    p->camDist = CAM_DIST_DEF;

    p->maxHp = 100.0f; p->hp  = p->maxHp;
    p->maxSta= 100.0f; p->sta = p->maxSta;
    p->maxMp =  60.0f; p->mp  = p->maxMp;

    p->level = 1; p->xp = 0; p->xpNext = 120; p->gold = 25;
    p->skillMelee = 5; p->skillMagic = 5;

    p->weapon = ITEM_RUSTY_SWORD;
    p->armor  = ITEM_NONE;

    InvAdd(p->inv, ITEM_RUSTY_SWORD, 1);
    InvAdd(p->inv, ITEM_POTION_HEALTH, 2);
    InvAdd(p->inv, ITEM_BREAD, 3);

    PlayerLoadModel(p);      /* dopo il memset: azzera hasModel e i puntatori */
}

Vector3 PlayerEye(const Player *p)
{
    return (Vector3){ p->pos.x, p->pos.y + PLAYER_EYE, p->pos.z };
}

Vector3 PlayerLookDir(const Player *p)
{
    return (Vector3){ cosf(p->pitch) * sinf(p->yaw),
                      sinf(p->pitch),
                      cosf(p->pitch) * cosf(p->yaw) };
}

void PlayerCamera(const Player *p, const World *w, Camera3D *cam)
{
    /* Leggero "head bob" quando si cammina: costa poco, aggiunge molto. In
     * terza persona darebbe il mal di mare, quindi vale solo in soggettiva. */
    float bob = (p->camMode == CAM_FIRST) ? sinf(p->bobPhase) * 0.055f : 0.0f;
    Vector3 eye = PlayerEye(p);
    eye.y += bob;
    Vector3 fwd = PlayerLookDir(p);

    cam->up   = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->fovy = 70.0f;
    cam->projection = CAMERA_PERSPECTIVE;

    if (p->camMode == CAM_FIRST) {
        cam->position = eye;
        cam->target   = Vector3Add(eye, fwd);
        return;
    }

    /* Terza persona: la camera arretra lungo la direzione della visuale.
     * Se il terreno si mette in mezzo (siamo in salita, o guardiamo in alto)
     * la distanza si accorcia, altrimenti la camera finisce sotto la collina.
     * Il campionamento e' grossolano - otto passi - ma il terreno e' una
     * funzione pura, quindi costa solo qualche WorldHeight() per frame. */
    const int STEPS = 8;
    float dist = p->camDist;
    for (int i = 1; i <= STEPS; i++) {
        float t = p->camDist * (float)i / (float)STEPS;
        Vector3 s = Vector3Subtract(eye, Vector3Scale(fwd, t));
        if (s.y < WorldHeight(w, s.x, s.z) + CAM_CLEARANCE) {
            dist = t - p->camDist / (float)STEPS;   /* fermati un passo prima */
            break;
        }
    }
    if (dist < 0.5f) dist = 0.5f;

    /* Scostamento in alto e a destra: senza, il giocatore sta esattamente sul
     * mirino e copre quello che si sta inquadrando. Il prezzo e' che l'asse
     * della camera non coincide piu' con la direzione di mira - qui sono 3
     * gradi - ma il combattimento non ne soffre: usa PlayerLookDir(), non la
     * camera. */
    Vector3 right = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };
    cam->position = Vector3Subtract(eye, Vector3Scale(fwd, dist));
    cam->position = Vector3Add(cam->position, Vector3Scale(right, CAM_SHOULDER));
    cam->position.y += CAM_RISE;

    float minY = WorldHeight(w, cam->position.x, cam->position.z) + CAM_CLEARANCE;
    if (cam->position.y < minY) cam->position.y = minY;

    cam->target = Vector3Add(eye, Vector3Scale(fwd, 4.0f));
}

/* Disegna il modello animato. Con uno scheletro si disegnano solo le mesh
 * skinnate (vedi PlayerLoadModel), altrimenti tutte. */
/* Trasformazione che porta una mesh dalla posa di riposo a quella corrente
 * seguendo un osso. E' la stessa composizione che raylib usa per le ossa in
 * UpdateModelAnimationBones(): replicarla garantisce che l'arma e il corpo si
 * muovano insieme. I vertici delle mesh agganciate sono gia' in spazio modello
 * (raylib ci cuoce dentro la trasformazione del nodo al caricamento), percio'
 * qui serve solo posaCorrente x inversa(posaDiRiposo). */
static Matrix BoneMatrix(const Player *p, int bone, const ModelAnimation *anim, int frame)
{
    Transform in  = p->model.bindPose[bone];
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

static void DrawPlayerModel(const Player *p, Color tint)
{
    float s = PLAYER_MODEL_SCALE;
    Matrix xform = MatrixMultiply(
        MatrixMultiply(MatrixScale(s, s, s),
                       MatrixRotateY(p->yaw + PLAYER_MODEL_YAW * DEG2RAD)),
        MatrixTranslate(p->pos.x, p->pos.y, p->pos.z));

    for (int i = 0; i < p->model.meshCount; i++) {
        if (p->anySkinned && p->model.meshes[i].boneIds == NULL) continue;

        Material mat = p->model.materials[p->model.meshMaterial[i]];
        Color keep = mat.maps[MATERIAL_MAP_DIFFUSE].color;
        /* il ciclo giorno/notte moltiplica la texture, come per i prop */
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(p->model.meshes[i], mat, xform);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = keep;
    }

    /* Arma, scudo, elmo: mesh senza pesi, che raylib lascerebbe nella posa di
     * riposo. Si spostano a mano con la trasformazione dell'osso. */
    const ModelAnimation *anim = NULL;
    int frame = 0;
    if (p->animCount > 0 && p->animOf[p->anim] >= 0) {
        anim  = &p->anims[p->animOf[p->anim]];
        frame = (int)p->animFrame;
        if (anim->frameCount > 0) frame %= anim->frameCount;
        else anim = NULL;
    }

    for (int k = 0; k < p->attachCount; k++) {
        int mi = p->attach[k].mesh;
        Matrix m = anim ? MatrixMultiply(BoneMatrix(p, p->attach[k].bone, anim, frame), xform)
                        : xform;
        Material mat = p->model.materials[p->model.meshMaterial[mi]];
        Color keep = mat.maps[MATERIAL_MAP_DIFFUSE].color;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(p->model.meshes[mi], mat, m);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = keep;
    }
}

void PlayerDraw(const Player *p, Color tint)
{
    if (p->camMode == CAM_FIRST) return;
    if (p->hasModel) { DrawPlayerModel(p, tint); return; }

    Color body = { (unsigned char)( 96 * tint.r / 255),
                   (unsigned char)(104 * tint.g / 255),
                   (unsigned char)(126 * tint.b / 255), 255 };
    Color skin = { (unsigned char)(216 * tint.r / 255),
                   (unsigned char)(186 * tint.g / 255),
                   (unsigned char)(152 * tint.b / 255), 255 };

    /* Stessa grammatica visiva dei bipedi in entity.c: capsula, testa, arma.
     * Il raggio e' piu' sottile di PLAYER_RADIUS, che serve alle collisioni:
     * disegnarlo a 0.45 m dava un personaggio a forma di botte che copriva
     * anche la spada. */
    const float BODY_R = 0.30f;
    Vector3 f = { sinf(p->yaw), 0.0f, cosf(p->yaw) };
    Vector3 r = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };

    Vector3 a = { p->pos.x, p->pos.y + 0.45f, p->pos.z };
    Vector3 b = { p->pos.x, p->pos.y + PLAYER_HEIGHT - 0.55f, p->pos.z };
    DrawCapsule(a, b, BODY_R, 10, 6, body);
    DrawSphere((Vector3){ p->pos.x, p->pos.y + PLAYER_HEIGHT - 0.22f, p->pos.z },
               0.24f, skin);

    /* Gambe: due parallelepipedi che oscillano su bobPhase, la stessa fase che
     * in soggettiva muove la testa. Non e' un'animazione vera, ma basta per
     * leggere il passo e per capire da che parte e' girato il personaggio. */
    Color legc = { (unsigned char)(body.r * 3 / 4), (unsigned char)(body.g * 3 / 4),
                   (unsigned char)(body.b * 3 / 4), 255 };
    float step = sinf(p->bobPhase) * 0.20f;
    for (int i = 0; i < 2; i++) {
        float side = (i == 0) ? 1.0f : -1.0f;
        Vector3 leg = { p->pos.x + r.x * 0.15f * side + f.x * step * side,
                        p->pos.y + 0.28f,
                        p->pos.z + r.z * 0.15f * side + f.z * step * side };
        DrawCube(leg, 0.17f, 0.56f, 0.17f, legc);
    }

    if (p->weapon != ITEM_NONE) {
        float sw = p->swing;                     /* 1 -> 0 durante il fendente */
        Vector3 hand = { p->pos.x + r.x * 0.42f + f.x * (0.20f + sw * 0.45f),
                         p->pos.y + PLAYER_HEIGHT * 0.58f - sw * 0.10f,
                         p->pos.z + r.z * 0.42f + f.z * (0.20f + sw * 0.45f) };
        DrawCube(hand, 0.08f, 0.85f - sw * 0.45f, 0.08f,
                 (Color){ (unsigned char)(200 * tint.r / 255),
                          (unsigned char)(202 * tint.g / 255),
                          (unsigned char)(212 * tint.b / 255), 255 });
    }
}

float PlayerAttackDamage(const Player *p)
{
    float base = (p->weapon != ITEM_NONE) ? ITEMS[p->weapon].power : 6.0f;
    return base * (1.0f + (float)p->skillMelee * 0.03f);
}

float PlayerArmorValue(const Player *p)
{
    return (p->armor != ITEM_NONE) ? ITEMS[p->armor].power : 0.0f;
}

void PlayerTakeDamage(Player *p, float dmg)
{
    float reduced = dmg - PlayerArmorValue(p) * 0.6f;
    if (reduced < dmg * 0.25f) reduced = dmg * 0.25f;   /* l'armatura non annulla */

    /* Parare costa vigore in proporzione al colpo assorbito. */
    if (p->blocking) {
        p->sta -= reduced * 0.35f;
        if (p->sta < 0.0f) p->sta = 0.0f;
        reduced *= BLOCK_DMG_MUL;
    }
    p->hp -= reduced;
    p->hurtFlash = 0.35f;
    if (p->hp < 0.0f) p->hp = 0.0f;
}

void PlayerAddXP(Player *p, int amount)
{
    p->xp += amount;
    while (p->xp >= p->xpNext) {
        p->xp -= p->xpNext;
        p->level++;
        p->xpNext = 100 + p->level * 60;
        p->maxHp  += 12.0f;  p->hp  = p->maxHp;
        p->maxSta +=  6.0f;  p->sta = p->maxSta;
        p->maxMp  +=  8.0f;  p->mp  = p->maxMp;
        p->skillMelee++;
        p->skillMagic++;
    }
}

bool PlayerUseItem(Player *p, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= MAX_INVENTORY) return false;
    int id = p->inv[slotIndex].id;
    if (id == ITEM_NONE) return false;

    switch (ITEMS[id].kind) {
        case IK_WEAPON: p->weapon = id; return true;
        case IK_ARMOR:  p->armor  = id; return true;
        case IK_POTION:
            if (id == ITEM_POTION_HEALTH) {
                p->hp = fminf(p->maxHp, p->hp + ITEMS[id].power);
            } else {
                p->mp = fminf(p->maxMp, p->mp + ITEMS[id].power);
            }
            InvRemove(p->inv, id, 1);
            return true;
        case IK_FOOD:
            p->hp  = fminf(p->maxHp,  p->hp  + ITEMS[id].power);
            p->sta = fminf(p->maxSta, p->sta + 20.0f);
            InvRemove(p->inv, id, 1);
            return true;
        default: return false;
    }
}

void PlayerUpdate(Player *p, World *w, float dt, bool controlsEnabled)
{
    /* ---- Rotazione della visuale ------------------------------------- */
    if (controlsEnabled) {
        Vector2 md = GetMouseDelta();
        p->yaw   -= md.x * MOUSE_SENS;
        p->pitch -= md.y * MOUSE_SENS;
        if (p->pitch >  1.50f) p->pitch =  1.50f;
        if (p->pitch < -1.50f) p->pitch = -1.50f;
    }

    /* ---- Visuale: prima o terza persona ------------------------------- */
    if (controlsEnabled) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (p->camMode == CAM_FIRST) {
                /* rotellina indietro: si esce dalla soggettiva */
                if (wheel < 0.0f) { p->camMode = CAM_THIRD; p->camDist = CAM_DIST_MIN; }
            } else {
                p->camDist -= wheel * CAM_ZOOM_STEP;
                if (p->camDist < CAM_DIST_MIN) p->camMode = CAM_FIRST;
                p->camDist = Clamp(p->camDist, CAM_DIST_MIN, CAM_DIST_MAX);
            }
        }
        if (IsKeyPressed(KEY_F))
            p->camMode = (p->camMode == CAM_FIRST) ? CAM_THIRD : CAM_FIRST;
    }

    /* ---- Direzioni sul piano orizzontale ------------------------------ */
    Vector3 fwd   = { sinf(p->yaw), 0.0f, cosf(p->yaw) };
    Vector3 right = { -cosf(p->yaw), 0.0f, sinf(p->yaw) };

    Vector3 wish = { 0 };
    if (controlsEnabled) {
        if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, fwd);
        if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, fwd);
        if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
        if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    }
    if (Vector3Length(wish) > 0.001f) wish = Vector3Normalize(wish);

    /* Guardia alzata: rallenta, consuma vigore e dimezza i danni in arrivo
     * (vedi PlayerTakeDamage). Non si para a mezz'aria. */
    p->blocking = controlsEnabled && IsKeyDown(KEY_LEFT_CONTROL) &&
                  p->onGround && p->sta > 2.0f;
    if (p->blocking) p->sta -= BLOCK_STA_DRAIN * dt;

    p->sprinting = controlsEnabled && IsKeyDown(KEY_LEFT_SHIFT) && !p->blocking &&
                   p->sta > 1.0f && Vector3Length(wish) > 0.1f;

    float speed = p->sprinting ? PLAYER_RUN : PLAYER_WALK;
    if (p->inWater) speed *= 0.6f;
    if (p->blocking) speed *= BLOCK_SPEED_MUL;

    p->moveSpeed = Vector3Length(wish) * speed;

    /* Stamina: si consuma correndo, si rigenera fermi. */
    if (p->sprinting) p->sta -= 16.0f * dt;
    else              p->sta += 12.0f * dt;
    p->sta = Clamp(p->sta, 0.0f, p->maxSta);

    /* Magicka e vita rigenerano lentamente - ma non da morti, altrimenti hp
     * risale sopra zero e l'animazione di morte non parte mai. */
    p->mp = fminf(p->maxMp, p->mp + 3.5f * dt);
    if (p->hp > 0.0f) p->hp = fminf(p->maxHp, p->hp + 0.6f * dt);

    /* ---- Integrazione del movimento ----------------------------------- */
    Vector3 next = p->pos;
    next.x += wish.x * speed * dt;
    next.z += wish.z * speed * dt;

    /* Le pendenze molto ripide non sono scalabili. */
    float hNow  = WorldHeight(w, p->pos.x, p->pos.z);
    float hNext = WorldHeight(w, next.x, next.z);
    if (p->onGround && (hNext - hNow) > 1.35f * speed * dt * 2.0f) {
        next.x = p->pos.x;
        next.z = p->pos.z;
    }

    p->pos.x = next.x;
    p->pos.z = next.z;

    /* Limiti del mondo. */
    p->pos.x = Clamp(p->pos.x, 2.0f, WORLD_SIZE - 2.0f);
    p->pos.z = Clamp(p->pos.z, 2.0f, WORLD_SIZE - 2.0f);

    WorldResolveCollision(w, &p->pos, PLAYER_RADIUS);

    /* ---- Gravita' e salto --------------------------------------------- */
    float ground = WorldHeight(w, p->pos.x, p->pos.z);
    p->inWater = (ground < SEA_LEVEL - 0.2f) && (p->pos.y < SEA_LEVEL);

    if (controlsEnabled && IsKeyPressed(KEY_SPACE) && p->onGround && p->sta > 10.0f) {
        p->vel.y = PLAYER_JUMP;
        p->sta  -= 10.0f;
        p->onGround = false;
    }

    p->vel.y -= GRAVITY * dt * (p->inWater ? 0.25f : 1.0f);
    p->pos.y += p->vel.y * dt;

    float floorY = fmaxf(ground, p->inWater ? SEA_LEVEL - 1.2f : ground);
    if (p->pos.y <= floorY) {
        /* Danno da caduta. */
        if (!p->onGround && p->vel.y < -18.0f && !p->inWater)
            PlayerTakeDamage(p, (-p->vel.y - 18.0f) * 3.0f);
        p->pos.y = floorY;
        p->vel.y = 0.0f;
        p->onGround = true;
    } else {
        p->onGround = false;
    }

    /* ---- Timer e animazioni ------------------------------------------- */
    if (p->attackCd  > 0.0f) p->attackCd  -= dt;
    if (p->castCd    > 0.0f) p->castCd    -= dt;
    if (p->hurtFlash > 0.0f) p->hurtFlash -= dt;
    if (p->swing     > 0.0f) p->swing     -= dt * 2.6f;

    if (p->onGround && Vector3Length(wish) > 0.1f)
        p->bobPhase += dt * (p->sprinting ? 12.0f : 7.5f);

    PlayerUpdateAnimation(p, dt);
}
