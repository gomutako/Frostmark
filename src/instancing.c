#include "instancing.h"
#include "light.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

/* Il dato che va sulla scheda per ogni istanza: 32 byte. La soluzione ovvia -
 * una mat4 per il modello piu' una mat3 per le normali - ne vorrebbe 100. Qui
 * bastano perche' in questo gioco non esistono rotazioni libere: ogni prop e
 * ogni pezzo d'edificio e' posizione, imbardata e scala, e il vertex shader
 * ricostruisce da questi sia la matrice del modello sia quella delle normali.
 *
 * Seno e coseno arrivano gia' calcolati: farli nel vertex shader vorrebbe dire
 * rifarli per ogni vertice di ogni istanza invece che una volta per istanza. */
typedef struct {
    float x, y, z, sinYaw;
    float sx, sy, sz, cosYaw;
} InstData;

/* Gli slot 8 e 9 sono liberi in ogni configurazione di raylib: 0-4 sono
 * posizione, UV, normale, colore e tangente, 5 le seconde UV, 6 gli indici,
 * 7 e 8 le ossa. Vedi il commento in assets/shaders/scene_inst.vs. */
#define INST_LOC_POS    8
#define INST_LOC_SCALE  9

#define INST_CAP_INIZIALE 256

struct InstBatch {
    Mesh     mesh;
    Material mat;
    unsigned int vao, ivbo;
    InstData *cpu;
    int count, cap;
    int elems;          /* indici da disegnare, 0 se la mesh non e' indicizzata */
};

/* Aggancia il buffer d'istanza al VAO del lotto. Si rifa' identica quando il
 * buffer cresce, perche' l'aggancio e' per id e l'id cambia. */
static void AgganciaIstanze(InstBatch *b)
{
    rlEnableVertexBuffer(b->ivbo);
    rlSetVertexAttribute(INST_LOC_POS, 4, RL_FLOAT, false, sizeof(InstData), 0);
    rlSetVertexAttributeDivisor(INST_LOC_POS, 1);
    rlEnableVertexAttribute(INST_LOC_POS);
    rlSetVertexAttribute(INST_LOC_SCALE, 4, RL_FLOAT, false, sizeof(InstData),
                         (int)offsetof(InstData, sx));
    rlSetVertexAttributeDivisor(INST_LOC_SCALE, 1);
    rlEnableVertexAttribute(INST_LOC_SCALE);
}

InstBatch *InstCreate(Mesh mesh, Material mat)
{
    Shader sh = LightInstShader();
    if (sh.id == 0) return NULL;              /* niente shader, niente lotto */
    if (mesh.vaoId == 0 || mesh.vboId == NULL) return NULL;   /* mesh non caricata */

    InstBatch *b = (InstBatch *)MemAlloc((unsigned int)sizeof(InstBatch));
    if (b == NULL) return NULL;

    b->mesh = mesh;
    /* Il materiale e' una copia: al chiamante serve il suo, con lo shader
     * normale, per il percorso non instanziato. */
    b->mat = mat;
    b->mat.shader = sh;
    b->elems = (mesh.indices != NULL) ? mesh.triangleCount * 3 : 0;

    b->cap = INST_CAP_INIZIALE;
    b->cpu = (InstData *)MemAlloc((unsigned int)(b->cap * (int)sizeof(InstData)));
    b->count = 0;

    b->vao = rlLoadVertexArray();
    rlEnableVertexArray(b->vao);

    /* Gli attributi della mesh: gli STESSI VBO, ma su un VAO diverso. */
    rlEnableVertexBuffer(mesh.vboId[0]);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);

    rlEnableVertexBuffer(mesh.vboId[1]);
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(1);

    rlEnableVertexBuffer(mesh.vboId[2]);
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(2);

    /* Colore e tangente possono mancare. Si da' allora un valore fisso, come
     * fa UploadMesh(): bianco per il colore, e zero per la tangente - il
     * fragment shader sa gia' che una tangente nulla vuol dire "usa la normale
     * del vertice". */
    if (mesh.vboId[3] != 0) {
        rlEnableVertexBuffer(mesh.vboId[3]);
        rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlEnableVertexAttribute(3);
    } else {
        float bianco[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        rlSetVertexAttributeDefault(3, bianco, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(3);
    }

    if (mesh.vboId[4] != 0) {
        rlEnableVertexBuffer(mesh.vboId[4]);
        rlSetVertexAttribute(4, 4, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(4);
    } else {
        float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(4, zero, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(4);
    }

    if (b->elems > 0) rlEnableVertexBufferElement(mesh.vboId[6]);

    b->ivbo = rlLoadVertexBuffer(NULL, b->cap * (int)sizeof(InstData), true);
    AgganciaIstanze(b);

    rlDisableVertexArray();
    return b;
}

void InstFree(InstBatch *b)
{
    if (b == NULL) return;
    /* La mesh e il materiale NON si scaricano: il lotto li usa, non li
     * possiede. Chi li ha caricati li scarica. */
    rlUnloadVertexBuffer(b->ivbo);
    rlUnloadVertexArray(b->vao);
    MemFree(b->cpu);
    MemFree(b);
}

void InstBegin(InstBatch *b)
{
    if (b != NULL) b->count = 0;
}

void InstTint(InstBatch *b, Color tint)
{
    if (b != NULL) b->mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
}

static void Cresci(InstBatch *b)
{
    b->cap *= 2;
    b->cpu = (InstData *)MemRealloc(b->cpu, (unsigned int)(b->cap * (int)sizeof(InstData)));

    /* Il buffer sulla scheda va rifatto piu' grande, e riagganciato al VAO:
     * l'aggancio e' per id, e l'id e' cambiato. */
    rlUnloadVertexBuffer(b->ivbo);
    b->ivbo = rlLoadVertexBuffer(NULL, b->cap * (int)sizeof(InstData), true);
    rlEnableVertexArray(b->vao);
    AgganciaIstanze(b);
    rlDisableVertexArray();
}

void InstAdd(InstBatch *b, Vector3 pos, float yawDeg, Vector3 scale)
{
    if (b == NULL) return;
    if (b->count == b->cap) Cresci(b);

    float a = yawDeg * DEG2RAD;
    InstData *d = &b->cpu[b->count++];
    d->x = pos.x; d->y = pos.y; d->z = pos.z; d->sinYaw = sinf(a);
    d->sx = scale.x; d->sy = scale.y; d->sz = scale.z; d->cosYaw = cosf(a);
}

void InstFlush(InstBatch *b)
{
    if (b == NULL || b->count == 0) return;

    rlUpdateVertexBuffer(b->ivbo, b->cpu, b->count * (int)sizeof(InstData), 0);
    rlEnableShader(b->mat.shader.id);

    /* Dentro BeginMode3D la modelview E' la vista: non c'e' matrice di modello
     * da comporre, quella arriva per istanza. Vale anche nel passaggio d'ombra,
     * dove BeginMode3D ha ricevuto la camera del sole. Le manda instancing.c
     * perche' raylib le spedisce da se' solo dentro DrawMesh(). */
    if (b->mat.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1)
        rlSetUniformMatrix(b->mat.shader.locs[SHADER_LOC_MATRIX_VIEW],
                           rlGetMatrixModelview());
    if (b->mat.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1)
        rlSetUniformMatrix(b->mat.shader.locs[SHADER_LOC_MATRIX_PROJECTION],
                           rlGetMatrixProjection());

    if (b->mat.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1) {
        Color c = b->mat.maps[MATERIAL_MAP_DIFFUSE].color;
        float col[4] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
        rlSetUniform(b->mat.shader.locs[SHADER_LOC_COLOR_DIFFUSE], col,
                     SHADER_UNIFORM_VEC4, 1);
    }

    /* Le texture del materiale, negli slot bassi. Quelli alti - dal 10 in su -
     * sono delle mappe d'ombra, che light.c lega una volta per fotogramma.
     *
     * Si legano solo le due mappe che scene.fs usa davvero, l'albedo e le
     * normali, invece di ciclare su tutte: raylib non espone il numero di
     * mappe di un materiale, e comunque legare quelle che lo shader non guarda
     * e' lavoro buttato. */
    static const int mappe[] = { MATERIAL_MAP_DIFFUSE, MATERIAL_MAP_NORMAL };
    for (int k = 0; k < (int)(sizeof mappe / sizeof *mappe); k++) {
        int i = mappe[k];
        if (b->mat.maps[i].texture.id == 0) continue;
        rlActiveTextureSlot(i);
        rlEnableTexture(b->mat.maps[i].texture.id);
        rlSetUniform(b->mat.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i,
                     SHADER_UNIFORM_INT, 1);
    }

    rlEnableVertexArray(b->vao);
    if (b->elems > 0) rlDrawVertexArrayElementsInstanced(0, b->elems, 0, b->count);
    else              rlDrawVertexArrayInstanced(0, b->mesh.vertexCount, b->count);
    rlDisableVertexArray();

    for (int k = 0; k < (int)(sizeof mappe / sizeof *mappe); k++) {
        int i = mappe[k];
        if (b->mat.maps[i].texture.id == 0) continue;
        rlActiveTextureSlot(i);
        rlDisableTexture();
    }
    rlDisableShader();
}
