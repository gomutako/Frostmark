/* ============================================================================
 * save.h - Salvataggio/caricamento su file binario.
 *
 * Salviamo SOLO il seme del mondo e lo stato del giocatore: il mondo viene
 * rigenerato identico al caricamento. E' il vantaggio della generazione
 * procedurale deterministica.
 * ========================================================================== */
#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
struct Game;

bool SaveGameToFile(struct Game *g, const char *path);
bool LoadGameFromFile(struct Game *g, const char *path);

#endif /* SAVE_H */
