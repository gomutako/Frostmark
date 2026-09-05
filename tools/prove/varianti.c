/* ============================================================================
 * varianti.c - Meta' del catalogo vegetale Poly Haven non e' un oggetto: e' un
 * set di individui affiancati. shrub_02 sono quattro cespugli in fila su sei
 * metri, periwinkle_plant sei piante su 1,2. Il gioco deve mostrarne UNO.
 *
 * Questa prova fissa il confine fra "sei piante affiancate" e "una pianta in
 * sei pezzi": la prima e' un set, la seconda no, e dopo LoadModel() l'unica
 * differenza visibile e' che nel secondo caso gli ingombri si sovrappongono -
 * Mesh di raylib non porta il nome.
 *
 * Niente OpenGL qui: il raggruppamento e' geometria pura, e una prova che non
 * chiede un contesto grafico gira anche dove le altre escono 77.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "prova.h"

/* Ingombri deliberatamente ASIMMETRICI: un ingombro cubico non si accorge se
 * si scambia l'altezza con la larghezza, ed e' lo stesso inganno del cubo
 * nella prova dell'instancing. */
static BoundingBox Box(float cx, float cz, float lx, float ly, float lz)
{
    BoundingBox b;
    b.min = (Vector3){ cx - lx * 0.5f, 0.0f, cz - lz * 0.5f };
    b.max = (Vector3){ cx + lx * 0.5f, ly,   cz + lz * 0.5f };
    return b;
}

int main(void)
{
    /* Quattro cespugli in fila, come shrub_02: separati lungo X. */
    BoundingBox fila[4] = {
        Box(0.0f, 0.0f, 0.8f, 1.1f, 0.6f),
        Box(2.0f, 0.0f, 0.9f, 1.4f, 0.7f),
        Box(4.0f, 0.0f, 0.7f, 0.9f, 0.5f),
        Box(6.0f, 0.0f, 1.0f, 1.6f, 0.8f),
    };
    int idx[4];
    MeshGroup gr[4];
    int n = MeshGroupSplit(fila, 4, idx, gr, 4);
    Ok("quattro cespugli in fila -> quattro varianti", n == 4);
    Ok("una mesh per variante", n == 4 && gr[0].count == 1 && gr[3].count == 1);

    /* L'ordine e' per X crescente, e deve reggere anche se l'array arriva
     * mescolato: e' l'ordine che decide quale indice tocca a quale variante, e
     * lo stesso mondo salvato deve mostrare gli stessi cespugli. */
    BoundingBox mesc[4] = { fila[2], fila[0], fila[3], fila[1] };
    n = MeshGroupSplit(mesc, 4, idx, gr, 4);
    int ordinato = (n == 4);
    for (int i = 1; i < n; i++)
        if (gr[i].box.min.x < gr[i - 1].box.min.x) ordinato = 0;
    Ok("i gruppi escono ordinati per X, comunque arrivino", ordinato);

    /* Una pianta sola in sei pezzi, come nettle_plant: gli ingombri si
     * sovrappongono perche' sono i materiali dello stesso individuo. Ogni
     * pezzo tocca solo il vicino immediato (non tutti gli altri): serve a
     * beccare un ToccaXZ che si accontenta di un contatto qualsiasi invece
     * di verificare quello vero, perche' con un contatto pieno su tutta la
     * fila la catena resterebbe unita comunque. */
    BoundingBox pianta[6];
    for (int i = 0; i < 6; i++)
        pianta[i] = Box(0.18f * (float)i, 0.0f, 0.2f, 1.2f + 0.1f * (float)i, 0.7f);
    int idx6[6];
    MeshGroup gr6[6];
    n = MeshGroupSplit(pianta, 6, idx6, gr6, 6);
    Ok("sei mesh sovrapposte -> una variante sola", n == 1);
    Ok("la variante unica tiene tutte le mesh", n == 1 && gr6[0].count == 6);

    /* Transitivita': A tocca B, B tocca C, A non tocca C. Sono un gruppo.
     * Il contatto fra A-B e fra B-C e' di striscio (stesso vincolo del
     * commento sulla pianta qui sotto): serve a beccare un ToccaXZ che
     * allarga il contatto invece di verificare quello vero. */
    BoundingBox cat[3] = {
        Box(0.0f,  0.0f, 0.2f, 1.0f, 1.0f),
        Box(0.18f, 0.0f, 0.2f, 1.2f, 1.0f),
        Box(0.36f, 0.0f, 0.2f, 0.8f, 1.0f),
    };
    int idx3[3];
    MeshGroup gr3[3];
    n = MeshGroupSplit(cat, 3, idx3, gr3, 3);
    Ok("il contatto e' transitivo: A-B-C fanno un gruppo", n == 1);

    /* L'ingombro del gruppo copre tutte le sue mesh, o la scala uscirebbe
     * sbagliata proprio sui set. */
    n = MeshGroupSplit(pianta, 6, idx6, gr6, 6);
    Ok("l'ingombro del gruppo e' l'unione delle sue mesh",
       n == 1 && gr6[0].box.max.y > 1.69f && gr6[0].box.max.y < 1.71f);

    /* Ogni mesh finisce in un gruppo e in uno solo. */
    n = MeshGroupSplit(fila, 4, idx, gr, 4);
    int visto[4] = { 0, 0, 0, 0 }, tutte = (n == 4);
    for (int g = 0; g < n; g++)
        for (int k = 0; k < gr[g].count; k++) visto[idx[gr[g].first + k]]++;
    for (int i = 0; i < 4; i++) if (visto[i] != 1) tutte = 0;
    Ok("ogni mesh sta in un gruppo e in uno solo", tutte);

    return ProveEsito();
}
