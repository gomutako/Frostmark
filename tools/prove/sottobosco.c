/* ============================================================================
 * sottobosco.c - I cinque prop di dettaglio stanno in una tabella, e la tabella
 * la leggono in due: il gioco per disegnare e collidere, il baker per emettere.
 * Se fosse incompleta o incoerente, un prop nascerebbe nel mondo cotto e non si
 * disegnerebbe - o peggio, sarebbe solido e invisibile.
 *
 * Niente OpenGL: e' una tabella, un po' di aritmetica e le due funzioni che la
 * leggono - la collisione del gioco e la generazione del baker. Includerli tutti
 * e tre nella stessa unita' di traduzione funziona: verificato compilando prima
 * di scrivere il piano, non ci sono simboli in conflitto.
 * ========================================================================== */
/* propdefs.h esplicito: al Task 2 world.c non lo include ancora, e la prova non
 * deve dipendere dall'ordine in cui i compiti arrivano. */
#include "../../src/propdefs.h"
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "../worldgen.c"
#include "prova.h"

int main(void)
{
    /* --- La tabella e' completa ----------------------------------------- *
     * Ogni tipo dopo PROP_CRYPT e' un prop di dettaglio e deve avere la sua
     * riga. Una riga dimenticata darebbe un prop che il baker emette e il gioco
     * non sa disegnare, senza un avviso. */
    int mancanti = 0, senzaFile = 0, senzaTaglia = 0, senzaDistanza = 0;
    for (int t = PROP_STUMP; t < PROP_COUNT; t++) {
        const PropDetail *d = PropDetailOf((PropType)t);
        if (d == NULL) { mancanti++; continue; }
        if (d->file == NULL || d->file[0] == '\0') senzaFile++;
        if (d->voluto <= 0.0f) senzaTaglia++;
        if (d->maxDist <= 0.0f) senzaDistanza++;
    }
    Ok("ogni tipo di dettaglio ha la sua riga", mancanti == 0);
    Ok("ogni riga dichiara un file",            senzaFile == 0);
    Ok("ogni riga dichiara una taglia",         senzaTaglia == 0);
    Ok("ogni riga dichiara una distanza",       senzaDistanza == 0);
    Ok("la tabella ha esattamente cinque righe", gPropDetailCount == 5);

    /* E i sette grandi NON sono di dettaglio: e' quel NULL a decidere quale
     * regola si applica, e se un albero finisse nella tabella perderebbe la sua
     * primitiva di riserva. */
    int intrusi = 0;
    for (int t = PROP_TREE; t <= PROP_CRYPT; t++)
        if (PropDetailOf((PropType)t) != NULL) intrusi++;
    Ok("i prop grandi non stanno nella tabella", intrusi == 0);

    /* --- Le frequenze --------------------------------------------------- */
    float somma = 0.0f;
    for (int i = 0; i < gPropDetailCount; i++) somma += gPropDetail[i].frequenza;
    Ok("le frequenze sommano a uno", fabsf(somma - 1.0f) < 0.001f);

    /* La scelta pesata deve RISPETTARE le frequenze, non solo pescarle tutte.
     * Diecimila estrazioni su una griglia regolare: il conto atteso di ogni tipo
     * e' la sua frequenza per diecimila, con un decimo di tolleranza. */
    int conta[PROP_COUNT];
    for (int i = 0; i < PROP_COUNT; i++) conta[i] = 0;
    const int N = 10000;
    for (int k = 0; k < N; k++)
        conta[PropDetailPick(((float)k + 0.5f) / (float)N)]++;

    int tutti = 1, fuori = 0;
    for (int i = 0; i < gPropDetailCount; i++) {
        int atteso = (int)(gPropDetail[i].frequenza * (float)N);
        int letto  = conta[gPropDetail[i].type];
        if (letto == 0) tutti = 0;
        if (abs(letto - atteso) > atteso / 10) fuori++;
    }
    Ok("la scelta pesca tutti e cinque i tipi", tutti);
    Ok("e li pesca nelle proporzioni dichiarate", fuori == 0);

    /* Il tronco deve restare RARO: e' l'unico solido, quindi la sua frequenza e'
     * anche quella degli ostacoli. Un bosco con un tronco caduto ogni due passi
     * non e' un bosco. */
    Ok("il tronco e' il piu' raro dei cinque",
       conta[PROP_LOG] * 2 < conta[PROP_BARK]);

    /* Gli estremi non devono leggere fuori dall'array. */
    Ok("r = 0 da' un tipo valido",
       PropDetailOf(PropDetailPick(0.0f)) != NULL);
    Ok("r appena sotto 1 da' un tipo valido",
       PropDetailOf(PropDetailPick(0.999999f)) != NULL);
    Ok("r = 1 non esce dall'array",
       PropDetailOf(PropDetailPick(1.0f)) != NULL);

    /* --- Solido e allungato --------------------------------------------- *
     * Chi ha una lunghezza dichiarata deve avere anche un raggio, o i due cerchi
     * della collisione avrebbero raggio zero e il tronco si attraverserebbe. */
    int incoerenti = 0;
    for (int i = 0; i < gPropDetailCount; i++)
        if (gPropDetail[i].lunghezza > 0.0f && gPropDetail[i].raggio <= 0.0f)
            incoerenti++;
    Ok("chi e' allungato e' anche solido", incoerenti == 0);

    /* E i due cerchi devono coprire il tronco senza lasciare buchi: stanno a
     * meta' lunghezza meno il raggio, quindi il raggio non puo' essere meno di
     * un quarto della lunghezza o in mezzo resterebbe un varco. */
    int scoperti = 0;
    for (int i = 0; i < gPropDetailCount; i++) {
        const PropDetail *d = &gPropDetail[i];
        if (d->lunghezza <= 0.0f) continue;
        float centro = d->lunghezza * 0.5f - d->raggio;
        if (centro > d->raggio) scoperti++;    /* i due cerchi non si toccano */
    }
    Ok("i due cerchi del tronco non lasciano un buco in mezzo", scoperti == 0);

    /* --- I cerchi del tronco -------------------------------------------- *
     * Un tronco e' lungo 4,05 m e spesso 1,06: un cerchio solo sul suo spessore
     * lascerebbe attraversare le punte, uno che lo copre tutto sarebbe un muro
     * invisibile largo quattro metri. Due cerchi sull'asse, orientati dalla
     * rotazione del prop.
     *
     * World e' grosso: static, o si rischia la pila. */
    static World mondo;
    Prop tronco = { 0 };
    tronco.pos   = (Vector3){ 50.0f, 0.0f, 50.0f };
    tronco.scale = 1.0f;
    tronco.rot   = 0.0f;
    tronco.type  = PROP_LOG;

    Vector3 c[2];
    float raggio = 0.0f;

    /* Senza il modello caricato non ci sono cerchi: niente asset, niente prop.
     * E' la meta' della regola che si dimentica, e senza di lei il giocatore
     * sbatte contro tronchi invisibili. */
    mondo.hasExtProp[PROP_LOG] = false;
    Ok("senza il modello il tronco non e' solido",
       PropDetailCircles(&mondo, &tronco, c, &raggio) == 0);

    mondo.hasExtProp[PROP_LOG] = true;
    int ncerchi = PropDetailCircles(&mondo, &tronco, c, &raggio);
    Ok("con il modello il tronco da' due cerchi", ncerchi == 2);
    Ok("del raggio dichiarato", fabsf(raggio - 1.10f) < 0.01f);

    /* I due cerchi stanno sull'asse del tronco, simmetrici sul centro. */
    float cdx = c[1].x - c[0].x, cdz = c[1].z - c[0].z;
    float cd  = sqrtf(cdx * cdx + cdz * cdz);
    Ok("i cerchi coprono la lunghezza del tronco",
       fabsf(cd - (4.05f - 2.0f * 1.10f)) < 0.01f);
    Ok("e sono simmetrici sul centro del prop",
       fabsf((c[0].x + c[1].x) * 0.5f - 50.0f) < 0.01f &&
       fabsf((c[0].z + c[1].z) * 0.5f - 50.0f) < 0.01f);

    /* Ruotato di 90 gradi l'asse gira con lui: se i cerchi restassero sull'asse
     * X il tronco bloccherebbe dalla parte sbagliata, e a occhio non si vede. */
    tronco.rot = 90.0f;
    PropDetailCircles(&mondo, &tronco, c, &raggio);
    float rdx = c[1].x - c[0].x, rdz = c[1].z - c[0].z;
    Ok("ruotato, i cerchi ruotano con lui",
       fabsf(rdx) < 0.01f && fabsf(fabsf(rdz) - (4.05f - 2.20f)) < 0.01f);

    /* Un prop di dettaglio non solido non da' cerchi. */
    Prop scaglia = tronco;
    scaglia.type = PROP_BARK;
    mondo.hasExtProp[PROP_BARK] = true;
    Ok("la corteccia si calpesta",
       PropDetailCircles(&mondo, &scaglia, c, &raggio) == 0);

    /* --- La densita' della seconda passata -------------------------------- *
     * Il sottobosco vive dove ci sono alberi da cui derivare. Zero dove non ce
     * ne sono: un tronco caduto in mezzo all'oceano e' un difetto che si vede
     * una volta sola, per caso, dopo mesi. */
    Ok("in foresta il sottobosco e' fitto",  SottoboscoDensity(BIOME_FOREST) > 0.30f);
    Ok("in collina e' meno",                 SottoboscoDensity(BIOME_HILL) <
                                             SottoboscoDensity(BIOME_FOREST));
    Ok("in pianura e' raro",                 SottoboscoDensity(BIOME_PLAINS) < 0.12f);
    Ok("sull'oceano non ce n'e'",            SottoboscoDensity(BIOME_OCEAN) == 0.0f);
    Ok("sulla spiaggia non ce n'e'",         SottoboscoDensity(BIOME_BEACH) == 0.0f);
    Ok("sulla neve non ce n'e'",             SottoboscoDensity(BIOME_SNOW) == 0.0f);

    /* Nessuna densita' sopra 1: sarebbe una cella su una, cioe' un tappeto. */
    int troppo = 0;
    for (int b = 0; b < BIOME_COUNT; b++)
        if (SottoboscoDensity((Biome)b) > 1.0f) troppo++;
    Ok("nessuna densita' sfonda l'uno", troppo == 0);

    /* --- Il tetto per chunk ---------------------------------------------- *
     * La prima passata riempie 10x10 celle al 95,5% in foresta, la seconda 8x8
     * alla sua densita', e un chunk di villaggio ha 9 case piu' la torre. La
     * somma peggiore deve stare sotto MAX_PROPS_PER_CHUNK, o il baker tronca in
     * silenzio e sparisce roba dal mondo. */
    int peggiore = (int)(100.0f * 0.955f)
                 + (int)(64.0f * SottoboscoDensity(BIOME_FOREST))
                 + 10;
    Ok("il chunk peggiore sta sotto il tetto", peggiore < MAX_PROPS_PER_CHUNK);

    return ProveEsito();
}
