#include "items.h"

const ItemDef ITEMS[ITEM_COUNT] = {
    /* name                kind        value power desc */
    { "-",                 IK_NONE,        0,  0.0f, "" },
    { "Spada arrugginita", IK_WEAPON,     10, 10.0f, "Vecchia ma affilata quanto basta." },
    { "Spada di ferro",    IK_WEAPON,     90, 18.0f, "Arma affidabile dei mercenari." },
    { "Ascia d'acciaio",   IK_WEAPON,    210, 27.0f, "Lenta da brandire, devastante." },
    { "Lama Antica",       IK_WEAPON,    900, 42.0f, "Pulsa di una luce fredda." },
    { "Giubba di cuoio",   IK_ARMOR,      60,  4.0f, "Protezione leggera." },
    { "Corazza di ferro",  IK_ARMOR,     260, 10.0f, "Pesante ma robusta." },
    { "Pozione di cura",   IK_POTION,     45, 45.0f, "Ripristina 45 punti vita." },
    { "Pozione di mana",   IK_POTION,     40, 40.0f, "Ripristina 40 punti magia." },
    { "Pagnotta",          IK_FOOD,        6, 12.0f, "Ripristina un po' di vita." },
    { "Erba curativa",     IK_MISC,       12,  0.0f, "Ingrediente alchemico." },
    { "Pelliccia di lupo", IK_MISC,       25,  0.0f, "Calda e richiesta dai mercanti." },
    { "Anello del bandito",IK_MISC,       55,  0.0f, "Bottino di dubbia provenienza." },
    { "Polvere d'ossa",    IK_MISC,       35,  0.0f, "Resti di un non-morto." },
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
