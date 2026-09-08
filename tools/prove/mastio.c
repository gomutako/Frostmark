/* ============================================================================
 * mastio.c - Un kit modulare arriva in UN file: modular_fort_01 spedisce venti
 * pezzi di fortezza in 45 primitive. Il gioco ne usa uno solo, indirizzato per
 * INDICE, e questa prova fissa le due cose che possono andare storte in
 * silenzio:
 *
 *   1. l'indice pesca il pezzo sbagliato perche' l'ordinamento non e' totale;
 *   2. l'indice pesca un pezzo diverso perche' il catalogo ha ricotto il file,
 *      e nessuno se ne accorge finche' non si guarda la torre.
 *
 * Niente OpenGL: e' geometria pura, e gira anche dove le prove di rendering
 * escono 77.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "prova.h"

/* Ingombri ASIMMETRICI: un ingombro cubico non si accorge se si scambia
 * l'altezza con la profondita', ed e' lo stesso inganno del cubo nella prova
 * dell'instancing. */
static BoundingBox Box(float x0, float z0, float lx, float ly, float lz)
{
    BoundingBox b;
    b.min = (Vector3){ x0,      0.0f, z0      };
    b.max = (Vector3){ x0 + lx, ly,   z0 + lz };
    return b;
}

int main(void)
{
    /* Quattro pezzi copiati dalle misure vere di modular_fort_01. I primi due
     * cominciano ALLA STESSA X - e' il pareggio che il file contiene davvero,
     * due volte - e li separa solo la Z. */
    BoundingBox pezzi[4] = {
        Box(10.00f, -34.00f,  4.17f,  8.53f, 14.56f),  /* wall_thick_straight_01 */
        Box(10.00f, -14.00f,  2.55f,  8.61f, 14.82f),  /* wall_thin_straight_02  */
        Box(19.00f, -34.00f,  3.12f,  7.53f, 14.56f),  /* wall_walkway_straight  */
        Box(32.20f, -33.95f, 15.84f, 13.50f, 15.84f),  /* tower_round            */
    };
    int idx[4];
    MeshGroup gr[4];
    int ng = MeshGroupSplit(pezzi, 4, idx, gr, 4);
    Ok("quattro pezzi separati -> quattro gruppi", ng == 4);

    /* Il pareggio su min.x si risolve sulla min.z - e va provato con l'array
     * MESCOLATO, o non si prova niente. L'ordinamento e' per selezione e a
     * parita' non scambia: i due pezzi in pareggio restano nell'ordine in cui
     * arrivano, quindi se arrivano gia' ordinati per Z il criterio mancante
     * darebbe lo stesso risultato di quello presente. Verificato sabotando: con
     * l'array in ordine il sabotaggio passava.
     *
     * Nel file vero l'ordine di arrivo lo decide il catalogo, e non e' una cosa
     * su cui appoggiarsi. */
    BoundingBox mesc[4] = { pezzi[3], pezzi[1], pezzi[2], pezzi[0] };
    int idxm[4];
    MeshGroup grm[4];
    int ngm = MeshGroupSplit(mesc, 4, idxm, grm, 4);
    Ok("mescolati, restano quattro gruppi", ngm == 4);
    Ok("il pareggio su min.x lo risolve la min.z (gruppo 0)",
       ngm == 4 && fabsf((grm[0].box.max.z - grm[0].box.min.z) - 14.56f) < 0.01f);
    Ok("il pareggio su min.x lo risolve la min.z (gruppo 1)",
       ngm == 4 && fabsf((grm[1].box.max.z - grm[1].box.min.z) - 14.82f) < 0.01f);

    /* E l'indice deve pescare la torre anche da un array mescolato: e' l'ordine
     * a decidere, non la posizione nel file. */
    Ok("mescolati, l'indice pesca lo stesso pezzo",
       MeshGroupPick(grm, ngm, 3) != NULL &&
       fabsf((MeshGroupPick(grm, ngm, 3)->box.max.y -
              MeshGroupPick(grm, ngm, 3)->box.min.y) - 13.50f) < 0.01f);

    /* La selezione per indice: nel file vero la torre e' il gruppo 12, qui il
     * 3. Si riconosce dalla taglia, non dalla posizione nell'array. */
    const MeshGroup *torre = MeshGroupPick(gr, ng, 3);
    Ok("l'indice pesca il pezzo giusto",
       torre != NULL && fabsf((torre->box.max.y - torre->box.min.y) - 13.50f) < 0.01f);

    /* Un indice che non esiste NON e' un errore da ignorare: un asset ricotto
     * puo' avere meno pezzi di quando fu misurato, e il chiamante deve poter
     * ripiegare. */
    Ok("un indice oltre i gruppi torna NULL",  MeshGroupPick(gr, ng, 4)  == NULL);
    Ok("un indice negativo torna NULL",        MeshGroupPick(gr, ng, -1) == NULL);

    /* La guardia contro l'asset ricotto. */
    Vector3 atteso = { 15.84f, 13.50f, 15.84f };
    Ok("la torre somiglia a quella dichiarata",
       MeshGroupSomiglia(torre, atteso, 0.20f));
    Ok("un bastione NON somiglia alla torre",
       !MeshGroupSomiglia(MeshGroupPick(gr, ng, 0), atteso, 0.20f));

    /* Lo scarto si misura per LATO, non sul volume. Il pezzo qui sotto ha lo
     * STESSO VOLUME della torre - largo il doppio, alto la meta' - e non le
     * somiglia per niente: un confronto sul volume lo accetterebbe.
     *
     * Il caso e' scelto cosi' dopo un sabotaggio: con un pezzo largo solo la
     * meta' anche il confronto sul volume cadeva, e la prova non distingueva
     * le due regole. */
    BoundingBox stortoBox = Box(0.0f, 0.0f, 31.68f, 6.75f, 15.84f);
    MeshGroup storto = { 0, 1, stortoBox };
    Ok("un pezzo dello stesso volume ma dei lati sbagliati cade",
       !MeshGroupSomiglia(&storto, atteso, 0.20f));

    /* E anche uno che sbaglia un lato solo, che e' il caso piu' facile. */
    BoundingBox scemoBox = Box(0.0f, 0.0f, 7.92f, 13.50f, 15.84f);
    MeshGroup scemo = { 0, 1, scemoBox };
    Ok("un pezzo giusto su due lati e sbagliato sul terzo cade",
       !MeshGroupSomiglia(&scemo, atteso, 0.20f));

    /* Dentro tolleranza si accetta: una ricottura che cambia l'asset di poco
     * non deve spegnere la torre. */
    BoundingBox vicinoBox = Box(0.0f, 0.0f, 16.60f, 14.10f, 16.60f);
    MeshGroup vicino = { 0, 1, vicinoBox };
    Ok("uno scarto del 5% resta dentro tolleranza",
       MeshGroupSomiglia(&vicino, atteso, 0.20f));

    /* --- Il ricentraggio tocca SOLO il pezzo scelto ----------------------
     * raylib fonde le trasformazioni dei nodi dentro i vertici, quindi ogni
     * pezzo porta cucito l'offset che lo mette in fila con gli altri
     * diciannove. Va tolto - ma solo al pezzo che si disegna: spostare anche
     * gli altri non si vedrebbe mai, perche' nessuno li disegna, e resterebbe
     * li' finche' qualcuno non ne usa un secondo.
     *
     * Model e Mesh sono strutture semplici: se ne costruisce una a mano, senza
     * LoadModel e quindi senza contesto grafico. RicentraPezzo non tocca la
     * scheda apposta. */
    float v0[6] = { 10.0f, 0.0f, -34.0f,  14.0f,  8.0f, -20.0f };
    /* I due vertici sono gli SPIGOLI dell'ingombro del pezzo: cosi' dopo il
     * ricentraggio si puo' controllare dove e' finito il pezzo, e non solo di
     * quanto si e' mosso. */
    float v1[6] = { 32.2f, 0.0f, -33.9f,  48.04f, 13.5f, -18.06f };
    float v2[6] = { 19.0f, 0.0f, -34.0f,  22.0f,  7.5f, -20.0f };
    float atteso0[6], atteso2[6];
    for (int i = 0; i < 6; i++) { atteso0[i] = v0[i]; atteso2[i] = v2[i]; }

    Mesh mesh[3];
    for (int i = 0; i < 3; i++) { mesh[i] = (Mesh){ 0 }; mesh[i].vertexCount = 2; }
    mesh[0].vertices = v0;
    mesh[1].vertices = v1;
    mesh[2].vertices = v2;

    Model fake = { 0 };
    fake.meshCount = 3;
    fake.meshes    = mesh;

    /* Il gruppo scelto e' la sola mesh 1. 'first' indicizza midx, non le mesh:
     * e' la convenzione che MeshGroupSplit riempie. */
    int       midx[3] = { 0, 1, 2 };
    MeshGroup scelto  = { 1, 1, Box(32.2f, -33.9f, 15.84f, 13.50f, 15.84f) };

    Vector3 o = MeshGroupOrigin(&scelto);
    int mosse = RicentraPezzo(&fake, midx, &scelto, o);

    Ok("ricentra una mesh sola", mosse == 1);
    /* La PROPRIETA', non lo spostamento: il pezzo deve restare APPOGGIATO A
     * TERRA e centrato in XZ. Confrontare i vertici con "l'originale meno o"
     * sarebbe una tautologia - passerebbe con qualunque o, compresa una
     * sbagliata - ed e' esattamente il sabotaggio che ha bocciato la prima
     * versione di questa riga. */
    Ok("dopo il ricentraggio il pezzo poggia a terra",
       fabsf(v1[1]) < 0.01f && fabsf(v1[4] - 13.50f) < 0.01f);
    Ok("dopo il ricentraggio il pezzo e' centrato in XZ",
       fabsf(v1[0] + v1[3]) < 0.01f && fabsf(v1[2] + v1[5]) < 0.01f);

    int fermi = 1;
    for (int i = 0; i < 6; i++)
        if (v0[i] != atteso0[i] || v2[i] != atteso2[i]) fermi = 0;
    Ok("gli altri pezzi non si muovono", fermi);

    /* Una mesh senza vertici non si conta e non si deferenzia: dopo LoadModel
     * non capita, ma chi chiama fa UpdateMeshBuffer() sulle mesh mosse, e
     * quella funzione deferenzia anche vboId. */
    mesh[1].vertices = NULL;
    Ok("una mesh senza vertici non si conta",
       RicentraPezzo(&fake, midx, &scelto, o) == 0);

    return ProveEsito();
}
