/* ============================================================================
 * meshgroup.h - Quali mesh di un modello sono lo stesso individuo.
 *
 * Meta' del catalogo vegetale Poly Haven e' fatta di SET: shrub_02 sono
 * quattro cespugli diversi in fila su sei metri, periwinkle_plant sei piante
 * affiancate su 1,2. Caricati come un oggetto solo, dove va un cespuglio ne
 * compaiono quattro in miniatura.
 *
 * Mesh di raylib 5.5 non porta il nome, quindi dopo LoadModel() l'unica
 * informazione rimasta e' la geometria. La regola e' una sola:
 *
 *     due mesh sono lo stesso individuo se i loro ingombri XZ si toccano.
 *
 * Distingue "sei piante affiancate" da "una pianta in sei pezzi" senza sapere
 * niente delle due: nel secondo caso i materiali stanno uno dentro l'altro.
 *
 * Qui non c'e' OpenGL e non c'e' raylib oltre ai tipi: e' la parte che si puo'
 * provare senza un contesto grafico.
 * ========================================================================== */
#ifndef MESHGROUP_H
#define MESHGROUP_H

#include "raylib.h"
#include <stdbool.h>

/* Oltre questo numero di mesh il raggruppamento si arrende e il chiamante
 * tratta il modello come un individuo solo. Il modello piu' composto visto nel
 * catalogo, grass_medium_01, ne ha diciassette. */
#define MESHGROUP_MAX 64

/* Un individuo: 'first' e 'count' indicizzano l'array riempito da
 * MeshGroupSplit(), 'box' e' l'ingombro di tutte le sue mesh. */
typedef struct {
    int         first, count;
    BoundingBox box;
} MeshGroup;

/* Divide n ingombri in gruppi. Due mesh finiscono nello stesso gruppo se i
 * loro ingombri XZ si toccano, e la relazione e' transitiva.
 *
 * 'idx' e 'out' li alloca il chiamante, almeno n elementi ciascuno: idx esce
 * con gli indici delle mesh raggruppati, out con i gruppi ORDINATI PER X
 * crescente - l'ordine decide quale indice tocca a quale variante e deve
 * essere lo stesso a ogni esecuzione e su ogni piattaforma.
 *
 * Torna il numero di gruppi, 0 se n non e' valido o supera MESHGROUP_MAX. */
int MeshGroupSplit(const BoundingBox *box, int n, int *idx,
                   MeshGroup *out, int maxGroups);

/* L'origine da portare a zero: centro XZ dell'ingombro e MINIMO Y. Il centro
 * in Y metterebbe mezza pianta sottoterra: le piante stanno appoggiate. */
Vector3 MeshGroupOrigin(const MeshGroup *g);

/* Sposta i vertici di -o. 'v' e' l'array di raylib: 3 float per vertice.
 * Si fa una volta al caricamento e non per fotogramma - l'alternativa era un
 * uniform per lotto e una sottrazione per vertice a ogni disegno. */
void MeshGroupRecenter(float *v, int vertexCount, Vector3 o);

/* Il moltiplicatore che porta il gruppo alla dimensione voluta in metri:
 * sull'altezza se perAltezza, altrimenti sul lato XZ maggiore. Torna 1 se
 * l'ingombro e' degenere. */
float MeshGroupScale(const MeshGroup *g, float voluto, bool perAltezza);

#endif /* MESHGROUP_H */
