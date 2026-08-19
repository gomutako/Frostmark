/* ============================================================================
 * dataid.h - Identificatori stabili per i dati di gioco.
 *
 * Oggi ITEM_IRON_SWORD e' l'indice 2 nella tabella ITEMS[], e quel 2 finisce
 * nei salvataggi. Quando le definizioni arriveranno da file esterni
 * (docs/05-piano-dati-esterni-e-motore.md), riordinare due righe cambierebbe in
 * silenzio il significato delle partite salvate.
 *
 * Ogni definizione ha quindi un identificatore testuale ("iron_sword"), e cio'
 * che viene persistito e' l'hash a 32 bit di quel testo, non la posizione. Gli
 * indici restano, ma solo come riferimento in memoria.
 *
 * L'hash e' FNV-1a a 32 bit: poche righe, ben distribuito, e - cosa che qui
 * conta piu' della velocita' - identico su qualunque piattaforma e compilatore,
 * quindi un salvataggio e' leggibile dove e' stato scritto e altrove.
 * ========================================================================== */
#ifndef DATAID_H
#define DATAID_H

#include <stddef.h>

/* 0 e' riservato a "nessun identificatore": non lo restituisce mai per una
 * stringa non vuota. */
static inline unsigned int DataId(const char *text)
{
    if (text == NULL || text[0] == '\0') return 0u;

    unsigned int h = 2166136261u;                 /* offset base FNV-1a */
    for (const char *c = text; *c; c++) {
        h ^= (unsigned int)(unsigned char)*c;
        h *= 16777619u;                           /* primo FNV */
    }
    return (h == 0u) ? 1u : h;                    /* mai 0 per errore */
}

#endif /* DATAID_H */
