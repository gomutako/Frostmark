#include "propdefs.h"
#include <stddef.h>

/* Misurati il 2026-09-08 sui .gltf a 1k di Poly Haven, ingombri dagli accessori
 * con le trasformazioni dei nodi applicate.
 *
 * 'voluto' e' sul LATO XZ MAGGIORE per tutti: sono oggetti che stanno a terra e
 * si misurano in larghezza, non in altezza - la stessa ragione per cui l'erba
 * usa perAltezza = false.
 *
 * 'maxDist' viene dalla taglia. Una scaglia da 61 cm a 140 m e' meno di un
 * pixel e costa una riga d'istanza: con cinque tipi nuovi sparsi su tutta la
 * foresta, le distanze sono la difesa del fotogramma.
 *
 * Solido e' solo il TRONCO: 4,05 x 1,06 e' un ostacolo che si vede e si aggira.
 * Il ceppo no, ed e' deliberato - e' alto 0,57 e il gioco non ha un gradino, per
 * cui un ostacolo al ginocchio che ferma di netto sembra un difetto.
 *
 * Ombra solo a tronco e ceppo: gli altri tre sono piatti a terra e non
 * proiettano niente che si veda, come erba e cespugli. */
const PropDetail gPropDetail[] = {
    /*  tipo         file                            voluto  perAlt  maxDist  raggio  lungh   ombra  freq  */
    { PROP_BARK,   "assets/models/corteccia.glb",     0.61f, false,   40.0f,   0.00f,  0.00f, false, 0.30f },
    { PROP_BRANCH, "assets/models/rami.glb",          1.30f, false,   50.0f,   0.00f,  0.00f, false, 0.30f },
    { PROP_ROOTS,  "assets/models/radici.glb",        1.74f, false,   60.0f,   0.00f,  0.00f, false, 0.17f },
    { PROP_STUMP,  "assets/models/ceppo.glb",         1.43f, false,  100.0f,   0.00f,  0.00f, true,  0.13f },
    { PROP_LOG,    "assets/models/tronco.glb",        4.05f, false,  140.0f,   1.10f,  4.05f, true,  0.10f },
};

const int gPropDetailCount = (int)(sizeof gPropDetail / sizeof gPropDetail[0]);

const PropDetail *PropDetailOf(PropType t)
{
    for (int i = 0; i < gPropDetailCount; i++)
        if (gPropDetail[i].type == t) return &gPropDetail[i];
    return NULL;
}

/* Scelta pesata. Le frequenze sommano 1, ma non ci si appoggia: con r == 1 o con
 * una somma appena sotto per errore di virgola mobile si torna l'ultima riga
 * invece di leggere fuori dall'array. */
PropType PropDetailPick(float r)
{
    float acc = 0.0f;
    for (int i = 0; i < gPropDetailCount; i++) {
        acc += gPropDetail[i].frequenza;
        if (r < acc) return gPropDetail[i].type;
    }
    return gPropDetail[gPropDetailCount - 1].type;
}
