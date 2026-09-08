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

    return ProveEsito();
}
