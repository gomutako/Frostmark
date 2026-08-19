#include "items.h"
#include <string.h>

const ItemDef ITEMS[ITEM_COUNT] = {
    /* id                  name                kind        value power desc */
    { "none",         "-",                 IK_NONE,        0,  0.0f, "" },
    { "rusty_sword",  "Spada arrugginita", IK_WEAPON,     10, 10.0f, "Vecchia ma affilata quanto basta." },
    { "iron_sword",   "Spada di ferro",    IK_WEAPON,     90, 18.0f, "Arma affidabile dei mercenari." },
    { "steel_axe",    "Ascia d'acciaio",   IK_WEAPON,    210, 27.0f, "Lenta da brandire, devastante." },
    { "ancient_blade","Lama Antica",       IK_WEAPON,    900, 42.0f, "Pulsa di una luce fredda." },
    { "leather_armor","Giubba di cuoio",   IK_ARMOR,      60,  4.0f, "Protezione leggera." },
    { "iron_armor",   "Corazza di ferro",  IK_ARMOR,     260, 10.0f, "Pesante ma robusta." },
    { "potion_health","Pozione di cura",   IK_POTION,     45, 45.0f, "Ripristina 45 punti vita." },
    { "potion_mana",  "Pozione di mana",   IK_POTION,     40, 40.0f, "Ripristina 40 punti magia." },
    { "bread",        "Pagnotta",          IK_FOOD,        6, 12.0f, "Ripristina un po' di vita." },
    { "herb",         "Erba curativa",     IK_MISC,       12,  0.0f, "Ingrediente alchemico." },
    { "wolf_pelt",    "Pelliccia di lupo", IK_MISC,       25,  0.0f, "Calda e richiesta dai mercanti." },
    { "bandit_ring",  "Anello del bandito",IK_MISC,       55,  0.0f, "Bottino di dubbia provenienza." },
    { "bone_dust",    "Polvere d'ossa",    IK_MISC,       35,  0.0f, "Resti di un non-morto." },
};

bool InvAdd(InvSlot *inv, int itemId, int qty)
{
    if (itemId <= ITEM_NONE || itemId >= ITEM_COUNT || qty <= 0) return false;

    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == itemId) { inv[i].qty += qty; return true; }

    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == ITEM_NONE) { inv[i].id = itemId; inv[i].qty = qty; return true; }

    return false;   /* inventario pieno */
}

bool InvRemove(InvSlot *inv, int itemId, int qty)
{
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv[i].id == itemId) {
            if (inv[i].qty < qty) return false;
            inv[i].qty -= qty;
            if (inv[i].qty <= 0) { inv[i].id = ITEM_NONE; inv[i].qty = 0; }
            return true;
        }
    }
    return false;
}

int InvCount(const InvSlot *inv, int itemId)
{
    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == itemId) return inv[i].qty;
    return 0;
}

int InvUsedSlots(const InvSlot *inv)
{
    int n = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) if (inv[i].id != ITEM_NONE) n++;
    return n;
}

float InvWeight(const InvSlot *inv)
{
    float wgt = 0.0f;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv[i].id == ITEM_NONE) continue;
        float unit;
        switch (ITEMS[inv[i].id].kind) {
            case IK_WEAPON: unit = 6.0f; break;
            case IK_ARMOR:  unit = 12.0f; break;
            case IK_POTION: unit = 0.5f; break;
            case IK_FOOD:   unit = 0.4f; break;
            default:        unit = 0.2f; break;
        }
        wgt += unit * (float)inv[i].qty;
    }
    return wgt;
}

/* ------------------------------------------------------------------------ */
/*  IDENTIFICATORI STABILI (vedi dataid.h)                                  */
/* ------------------------------------------------------------------------ */

int ItemFind(const char *id)
{
    if (id == NULL) return -1;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (strcmp(ITEMS[i].id, id) == 0) return i;
    return -1;
}

unsigned int ItemStableId(int item)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0u;
    return DataId(ITEMS[item].id);
}

int ItemFromStableId(unsigned int stable)
{
    if (stable == 0u) return ITEM_NONE;
    for (int i = 1; i < ITEM_COUNT; i++)
        if (DataId(ITEMS[i].id) == stable) return i;
    return ITEM_NONE;
}
