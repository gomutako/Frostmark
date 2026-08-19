/* ============================================================================
 * gamedata.h - Caricamento di tutti i dati di gioco.
 *
 * Nessun dato ha un valore di ripiego nel codice: se i file mancano o
 * contengono errori, il gioco non si avvia. L'ordine di caricamento conta,
 * perche' alcuni dati referenziano altri (il negozio cita gli oggetti).
 *
 * Vedi docs/05-piano-dati-esterni-e-motore.md, fasi 1 e 2.
 * ========================================================================== */
#ifndef GAMEDATA_H
#define GAMEDATA_H

#include <stdbool.h>

/* Carica tutto. Non si ferma al primo errore: elenca tutti i problemi con file
 * e riga, perche' correggerne uno alla volta e' inutilmente lento.
 * Il conteggio dei problemi si legge con DataProblemCount(). */
bool GameDataLoad(void);

#endif /* GAMEDATA_H */
