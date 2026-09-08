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

/* Quale gruppo tocca a un pezzo dichiarato per INDICE. Torna NULL se l'indice
 * non esiste.
 *
 * L'indice e' RIPRODUCIBILE - MeshGroupSplit ordina per min.x e, a parita', per
 * min.z - ma non e' STABILE NEL TEMPO: se il catalogo ricuoce il file e
 * riordina i pezzi, lo stesso indice pesca un altro oggetto. Un indice fuori
 * dai gruppi non e' quindi un errore da ignorare: e' un ripiego. */
const MeshGroup *MeshGroupPick(const MeshGroup *g, int ng, int pezzo);

/* L'ingombro del gruppo somiglia a quello dichiarato? 'tolleranza' e' RELATIVA
 * e vale PER LATO: 0,2 accetta un quinto di scarto su ognuno dei tre.
 *
 * E' l'altra meta' della difesa contro un asset ricotto: l'indice dice dove
 * guardare, questa dice se cio' che si e' trovato e' ancora quella cosa. Per
 * lato e non sul volume, o un pezzo largo il doppio e alto la meta' passerebbe. */
bool MeshGroupSomiglia(const MeshGroup *g, Vector3 atteso, float tolleranza);

#endif /* MESHGROUP_H */
