/* ============================================================================
 * propdefs.h - I prop di DETTAGLIO in una tabella sola.
 *
 * Aggiungere un prop "grande" tocca sei punti: l'enum, gExtProp, il switch
 * delle distanze, il switch del ripiego procedurale, la lista di chi non fa
 * ombra, e l'emissione nel baker. Per i cinque del sottobosco sarebbero trenta
 * modifiche, e una di quelle chiederebbe di inventare una primitiva procedurale
 * per una scaglia di corteccia da 20 cm.
 *
 * Qui invece un pezzo di sottobosco e' UNA RIGA, e la stessa riga la leggono il
 * gioco - per caricare, disegnare e collidere - e il baker, per emettere.
 *
 * I sette tipi grandi NON stanno qui: PropDetailOf() torna NULL per loro, ed e'
 * quel NULL a distinguere le due regole.
 * ========================================================================== */
#ifndef PROPDEFS_H
#define PROPDEFS_H

#include "worldtypes.h"
#include <stdbool.h>

typedef struct {
    PropType    type;
    const char *file;
    float       voluto;      /* taglia in metri                              */
    bool        perAltezza;  /* su cosa si misura: false = lato XZ maggiore   */
    float       maxDist;     /* oltre, non si disegna                        */
    float       raggio;      /* collisione: 0 = si calpesta                  */
    float       lunghezza;   /* >0: oggetto allungato, due cerchi sull'asse  */
    bool        ombra;       /* proietta ombra?                              */
    float       frequenza;   /* quota fra i cinque; la somma vale 1          */
} PropDetail;

extern const PropDetail gPropDetail[];
extern const int        gPropDetailCount;

/* La riga di un tipo, o NULL se non e' un prop di dettaglio. */
const PropDetail *PropDetailOf(PropType t);

/* Quale dei cinque, per 'r' in [0,1). Pesata sulle frequenze. */
PropType PropDetailPick(float r);

#endif /* PROPDEFS_H */
