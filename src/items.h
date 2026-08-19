/* ============================================================================
 * items.h - Database statico degli oggetti + inventario del giocatore.
 * ========================================================================== */
#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>
#include "config.h"
#include "dataid.h"

typedef enum {
    IK_NONE, IK_WEAPON, IK_ARMOR, IK_POTION, IK_FOOD, IK_MISC
} ItemKind;

/* "Nessun oggetto" e' una sentinella, non un dato: e' sempre l'indice 0.
 * Tutti gli altri oggetti arrivano da assets/data/items.txt e si cercano per
 * identificatore con ItemFind(). */
#define ITEM_NONE 0

typedef struct {
    char     id[32];     /* identificatore stabile, es. "iron_sword" */
    char     name[40];
    ItemKind kind;
    int      value;      /* prezzo base in monete d'oro */
    float    power;      /* danno (armi), riduzione (armature), cura... */
    char     desc[140];
} ItemDef;

/* Catalogo caricato: ITEMS[0] e' sempre l'oggetto nullo. */
extern ItemDef ITEMS[MAX_ITEMS];
int  ItemCount(void);

/* Carica il catalogo. false se il file manca o contiene errori: i problemi
 * vengono elencati con file e riga, e il gioco non deve avviarsi. */
bool ItemsLoad(const char *path);

/* Indice dell'oggetto dato il suo identificatore, -1 se non esiste. */
int ItemFind(const char *id);
/* Identificatore stabile da persistere, e ritorno. Fuori intervallo -> 0. */
unsigned int ItemStableId(int item);
int ItemFromStableId(unsigned int stable);   /* ITEM_NONE se sconosciuto */

typedef struct { int id; int qty; } InvSlot;

/* Operazioni sull'inventario (array di MAX_INVENTORY slot). */
bool InvAdd(InvSlot *inv, int itemId, int qty);
bool InvRemove(InvSlot *inv, int itemId, int qty);
int  InvCount(const InvSlot *inv, int itemId);
int  InvUsedSlots(const InvSlot *inv);
float InvWeight(const InvSlot *inv);

#endif /* ITEMS_H */
