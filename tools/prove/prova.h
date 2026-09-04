/* ============================================================================
 * prova.h - Asserzioni minime per le prove di Frostmark.
 *
 * Non c'e' un framework e non serve: una prova qui e' un eseguibile che stampa
 * una riga per controllo ed esce non-zero se qualcosa non torna. Si lanciano
 * tutte con 'make prove', dalla radice del repo - caricano gli asset per
 * percorso relativo.
 *
 * Le prove includono il .c che provano (#include "../../src/light.c") perche'
 * cio' che vale la pena provare e' quasi sempre 'static': la superficie
 * pubblica di questi moduli e' piccola apposta, e le decisioni difficili
 * stanno dentro.
 *
 * Le funzioni sono 'static inline' e non solo 'static': una prova che non le
 * usa tutte non deve dare avvisi, e il progetto compila con -Wall -Wextra e
 * zero avvisi.
 *
 * Chi non trova un contesto OpenGL esce con 77, e 'make prove' lo conta come
 * saltata invece che fallita: su una macchina senza GPU accessibile una prova
 * di rendering non e' rotta, e' solo impossibile.
 * ========================================================================== */
#ifndef PROVA_H
#define PROVA_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PROVA_SALTATA 77

static int gProveFallite = 0;

static inline void Ok(const char *cosa, int cond)
{
    printf("%-52s %s\n", cosa, cond ? "ok" : "FALLITO");
    if (!cond) gProveFallite++;
}

/* Per i valori misurati: fra due GPU un pixel puo' differire di uno. */
static inline void Near(const char *cosa, int letto, int atteso, int tolleranza)
{
    int c = abs(letto - atteso) <= tolleranza;
    printf("%-52s atteso %3d, letto %3d  %s\n", cosa, atteso, letto,
           c ? "ok" : "FALLITO");
    if (!c) gProveFallite++;
}

static inline int ProveEsito(void)
{
    printf("\n%s\n", gProveFallite ? "PROVA FALLITA" : "tutto a posto");
    return gProveFallite ? 1 : 0;
}

#endif /* PROVA_H */
