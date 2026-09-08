#include "meshgroup.h"
#include <math.h>
#include <stddef.h>

/* Si TOCCANO, non "si sovrappongono": due ingombri che si sfiorano sono lo
 * stesso individuo. Il rischio e' dichiarato nello spec - due varianti
 * modellate a contatto finirebbero insieme - e si vedrebbe subito, perche'
 * comparirebbero sempre in coppia. */
static bool ToccaXZ(BoundingBox a, BoundingBox b)
{
    return a.min.x <= b.max.x && b.min.x <= a.max.x &&
           a.min.z <= b.max.z && b.min.z <= a.max.z;
}

static BoundingBox Unione(BoundingBox a, BoundingBox b)
{
    BoundingBox u;
    u.min.x = fminf(a.min.x, b.min.x);
    u.min.y = fminf(a.min.y, b.min.y);
    u.min.z = fminf(a.min.z, b.min.z);
    u.max.x = fmaxf(a.max.x, b.max.x);
    u.max.y = fmaxf(a.max.y, b.max.y);
    u.max.z = fmaxf(a.max.z, b.max.z);
    return u;
}

/* Insiemi disgiunti con compressione del percorso: il contatto e' transitivo,
 * e una catena di mesh che si sfiorano a due a due e' un individuo solo. */
static int Radice(int *p, int i)
{
    while (p[i] != i) { p[i] = p[p[i]]; i = p[i]; }
    return i;
}

/* Ordine deterministico: X crescente, e a parita' di X la Z. Senza il secondo
 * criterio due varianti allineate sulla stessa X potrebbero scambiarsi di
 * posto fra un compilatore e l'altro, e lo stesso mondo salvato mostrerebbe
 * cespugli diversi. */
static bool PrimaDi(BoundingBox a, BoundingBox b)
{
    if (a.min.x != b.min.x) return a.min.x < b.min.x;
    return a.min.z < b.min.z;
}

int MeshGroupSplit(const BoundingBox *box, int n, int *idx,
                   MeshGroup *out, int maxGroups)
{
    if (box == NULL || idx == NULL || out == NULL) return 0;
    if (n <= 0 || n > MESHGROUP_MAX || maxGroups <= 0) return 0;

    int padre[MESHGROUP_MAX];
    for (int i = 0; i < n; i++) padre[i] = i;

    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (ToccaXZ(box[i], box[j])) padre[Radice(padre, i)] = Radice(padre, j);

    /* Una radice per gruppo, nell'ordine in cui compare. */
    int gruppoDi[MESHGROUP_MAX];
    for (int i = 0; i < n; i++) gruppoDi[i] = -1;

    int ng = 0;
    for (int i = 0; i < n; i++) {
        int r = Radice(padre, i);
        if (gruppoDi[r] == -1) {
            if (ng >= maxGroups) return 0;   /* il chiamante torna a una variante */
            gruppoDi[r]  = ng;
            out[ng].box   = box[i];
            out[ng].first = 0;
            out[ng].count = 0;
            ng++;
        } else {
            out[gruppoDi[r]].box = Unione(out[gruppoDi[r]].box, box[i]);
        }
        out[gruppoDi[r]].count++;
    }

    /* Ordinamento per selezione: i gruppi sono pochi, e serve la permutazione
     * per rimappare le mesh, non solo l'array ordinato. */
    int ordine[MESHGROUP_MAX];
    for (int k = 0; k < ng; k++) ordine[k] = k;
    for (int a = 0; a < ng; a++)
        for (int b = a + 1; b < ng; b++)
            if (PrimaDi(out[ordine[b]].box, out[ordine[a]].box)) {
                int t = ordine[a]; ordine[a] = ordine[b]; ordine[b] = t;
            }

    MeshGroup ord[MESHGROUP_MAX];
    int posDi[MESHGROUP_MAX], scritti[MESHGROUP_MAX];
    int acc = 0;
    for (int k = 0; k < ng; k++) {
        ord[k]              = out[ordine[k]];
        posDi[ordine[k]]    = k;
        ord[k].first        = acc;
        acc                += ord[k].count;
        scritti[k]          = 0;
    }

    for (int i = 0; i < n; i++) {
        int g = posDi[gruppoDi[Radice(padre, i)]];
        idx[ord[g].first + scritti[g]++] = i;
    }

    for (int k = 0; k < ng; k++) out[k] = ord[k];
    return ng;
}

Vector3 MeshGroupOrigin(const MeshGroup *g)
{
    Vector3 o;
    o.x = (g->box.min.x + g->box.max.x) * 0.5f;
    o.y = g->box.min.y;
    o.z = (g->box.min.z + g->box.max.z) * 0.5f;
    return o;
}

void MeshGroupRecenter(float *v, int vertexCount, Vector3 o)
{
    if (v == NULL) return;
    for (int i = 0; i < vertexCount; i++) {
        v[i * 3 + 0] -= o.x;
        v[i * 3 + 1] -= o.y;
        v[i * 3 + 2] -= o.z;
    }
}

float MeshGroupScale(const MeshGroup *g, float voluto, bool perAltezza)
{
    float dim = perAltezza
                ? g->box.max.y - g->box.min.y
                : fmaxf(g->box.max.x - g->box.min.x, g->box.max.z - g->box.min.z);
    return (dim > 1e-4f) ? voluto / dim : 1.0f;
}

const MeshGroup *MeshGroupPick(const MeshGroup *g, int ng, int pezzo)
{
    if (g == NULL || pezzo < 0 || pezzo >= ng) return NULL;
    return &g[pezzo];
}

bool MeshGroupSomiglia(const MeshGroup *g, Vector3 atteso, float tolleranza)
{
    if (g == NULL || tolleranza < 0.0f) return false;

    float lato[3] = { g->box.max.x - g->box.min.x,
                      g->box.max.y - g->box.min.y,
                      g->box.max.z - g->box.min.z };
    float att[3]  = { atteso.x, atteso.y, atteso.z };

    for (int i = 0; i < 3; i++) {
        /* Un lato atteso nullo non e' una dichiarazione, e' una riga vuota:
         * accettarlo vorrebbe dire accettare qualunque cosa. */
        if (att[i] <= 1e-4f) return false;
        if (fabsf(lato[i] - att[i]) > att[i] * tolleranza) return false;
    }
    return true;
}
