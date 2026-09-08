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

    /* --- Un numero, due usi ----------------------------------------------
     * La spinta del giocatore e il taglio della camera devono leggere la
     * STESSA taglia. Se divergessero, la camera entrerebbe in un muro che il
     * giocatore non puo' attraversare - ed e' lo stesso motivo per cui
     * StairTop() serve sia a chi cammina sulla rampa sia a chi ci sbatte
     * contro. */
    /* World e' grosso: static, o si rischia la pila. */
    static World mondo;
    Prop  torreProp = { 0 };
    torreProp.type   = PROP_TOWER;
    torreProp.scale  = 1.0f;
    torreProp.radius = 3.0f;          /* il raggio cotto nel mondo */

    /* Le due funzioni valgono per il MASTIO: i due chiamanti le invocano solo
     * dentro 'if (hasKeep)', e senza mastio i numeri di prima - 3,0 cotto per
     * la spinta, 1,6 e 12,0 per la scatola della camera - restano scritti dove
     * sono sempre stati. Un valore di ripiego qui dentro sarebbe codice che
     * non legge nessuno, e una prova che lo controlla proverebbe il nulla. */
    mondo.hasKeep  = true;
    mondo.keepHalf = 7.92f;
    mondo.keepHigh = 13.50f;
    Ok("la spinta usa la semiampiezza misurata",
       fabsf(TowerHalf(&mondo, &torreProp) - 7.92f) < 0.01f);
    Ok("l'altezza e' quella misurata, non la semiampiezza",
       fabsf(TowerHigh(&mondo, &torreProp) - 13.50f) < 0.01f);

    /* La scala dell'istanza vale su entrambi, o una torre rimpicciolita
     * spingerebbe alla taglia di quella intera. */
    torreProp.scale = 0.5f;
    Ok("la scala dell'istanza vale sulla semiampiezza",
       fabsf(TowerHalf(&mondo, &torreProp) - 3.96f) < 0.01f);
    Ok("la scala dell'istanza vale sull'altezza",
       fabsf(TowerHigh(&mondo, &torreProp) - 6.75f) < 0.01f);

    /* Il tetto che fissa la taglia: l'anziano nasce a 14 m dal centro del
     * villaggio, ed e' cotto nel mondo come i prop. Una semiampiezza oltre gli
     * 8 m lo farebbe nascere dentro la pietra. */
    torreProp.scale = 1.0f;
    Ok("la semiampiezza sta sotto gli 8 m che l'anziano impone",
       TowerHalf(&mondo, &torreProp) < 8.0f);

    /* --- La spinta vera, non solo il numero --------------------------------
     * WorldResolveCollision gira sui chunk, ma non tocca la GPU: un chunk si
     * costruisce a mano e la prova resta senza contesto grafico.
     *
     * Il caso che conta e' il CENTRO ESATTO. Il conto generico si arrende
     * quando la distanza e' zero, perche' non c'e' una direzione in cui
     * spingere: su un prop da 3 m e' un bersaglio stretto, sul mastio - largo
     * 15,84 e piantato al centro del villaggio - non lo e'. Misurato sul mondo
     * vero: senza il ramo apposta, chi finisce sul centro ci resta. */
    torreProp.scale = 1.0f;
    torreProp.pos   = (Vector3){ 100.0f, 0.0f, 100.0f };

    mondo.chunks[0].active    = true;
    mondo.chunks[0].cx        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].cz        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].propCount = 1;
    mondo.chunks[0].props[0]  = torreProp;

    Vector3 dentro = { 100.0f, 0.0f, 100.0f };      /* il centro esatto */
    WorldResolveCollision(&mondo, &dentro, 0.35f);
    float dOut = sqrtf((dentro.x - 100.0f) * (dentro.x - 100.0f) +
                       (dentro.z - 100.0f) * (dentro.z - 100.0f));
    Ok("dal centro esatto si viene spinti fuori lo stesso",
       dOut >= 7.92f + 0.35f - 0.01f);

    /* E di fianco, che e' il caso normale: 7,92 di pietra piu' il raggio di chi
     * cammina. */
    Vector3 vicino2 = { 102.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &vicino2, 0.35f);
    float dLato = sqrtf((vicino2.x - 100.0f) * (vicino2.x - 100.0f) +
                        (vicino2.z - 100.0f) * (vicino2.z - 100.0f));
    Ok("di fianco si resta fuori di semiampiezza piu' raggio",
       fabsf(dLato - (7.92f + 0.35f)) < 0.01f);

    /* Senza mastio la torre torna al cerchio cotto nel mondo: 3,0 + 0,35. E'
     * la promessa del ripiego, e va provata o non e' una promessa. */
    mondo.hasKeep = false;
    Vector3 kit = { 102.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &kit, 0.35f);
    float dKit = sqrtf((kit.x - 100.0f) * (kit.x - 100.0f) +
                       (kit.z - 100.0f) * (kit.z - 100.0f));
    Ok("senza mastio si torna al raggio cotto nel mondo",
       fabsf(dKit - (3.0f + 0.35f)) < 0.01f);

    return ProveEsito();
}
