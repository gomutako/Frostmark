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

/* Gli ID sono indici nell'array ITEMS[]: 0 = "nessun oggetto". */
enum {
    ITEM_NONE = 0,
    ITEM_RUSTY_SWORD, ITEM_IRON_SWORD, ITEM_STEEL_AXE, ITEM_ANCIENT_BLADE,
    ITEM_LEATHER_ARMOR, ITEM_IRON_ARMOR,
    ITEM_POTION_HEALTH, ITEM_POTION_MANA, ITEM_BREAD,
    ITEM_HERB, ITEM_WOLF_PELT, ITEM_BANDIT_RING, ITEM_BONE_DUST,
    ITEM_COUNT
};

typedef struct {
    const char *id;      /* identificatore stabile, es. "iron_sword" */
    const char *name;
    ItemKind    kind;
    int         value;   /* prezzo base in monete d'oro */
    float       power;   /* danno (armi), riduzione danno (armature), cura... */
    const char *desc;
} ItemDef;

extern const ItemDef ITEMS[ITEM_COUNT];

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
